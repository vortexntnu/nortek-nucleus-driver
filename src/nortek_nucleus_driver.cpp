#include "nortek_nucleus_driver.hpp"
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include "nortek_nucleus_messages.hpp"

namespace nortek::parser {

template <typename T>
T read_from_buffer(const uint8_t* data, std::size_t len, std::size_t offset) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "read_from_buffer requires trivially copyable types");

    if (offset + sizeof(T) > len) {
        return {};
    }

    T value{};
    std::memcpy(&value, data + offset, sizeof(T));
    return value;
}

SpectrumDatagram parse_spectrum_data(const uint8_t* data, std::size_t len) {
    SpectrumDataV3 spectrum_data =
        read_from_buffer<SpectrumDataV3>(data, len, 0);

    const std::size_t data_offset = spectrum_data.data_offset;

    const uint16_t bins = spectrum_data.num_beam_bins & 0x1FFF;
    const uint8_t beams = spectrum_data.num_beam_bins >> 13;

    SpectrumFrequencyHeader spectrum_freq_header =
        read_from_buffer<SpectrumFrequencyHeader>(data, len, data_offset);

    std::vector<int16_t> spectrum_freq_data(bins * beams);
    const std::size_t num_bytes = sizeof(int16_t) * bins * beams;
    std::memcpy(spectrum_freq_data.data(), data + data_offset + 64, num_bytes);

    SpectrumDatagram spectrum_datagram{};
    spectrum_datagram.spectrum = spectrum_data;
    spectrum_datagram.freq_header = spectrum_freq_header;
    spectrum_datagram.freq_data = spectrum_freq_data;

    return spectrum_datagram;
}

CurrentProfileDatagram parse_current_profile_data(const uint8_t* data,
                                                  std::size_t len,
                                                  std::size_t data_offset) {
    CurrentProfileData current_profile_data =
        nortek::parser::read_from_buffer<CurrentProfileData>(
            data, len, sizeof(CommonData));

    const uint16_t num_cells = current_profile_data.num_cells;

    std::vector<CurrentProfileVelocityData> velocity_data(num_cells);
    std::memcpy(velocity_data.data(), data + data_offset,
                num_cells * sizeof(CurrentProfileVelocityData));
    data_offset += num_cells * sizeof(CurrentProfileVelocityData);

    std::vector<CurrentProfileAmplitudeData> amplitude_data(num_cells);
    std::memcpy(amplitude_data.data(), data + data_offset,
                num_cells * sizeof(CurrentProfileAmplitudeData));
    data_offset += num_cells * sizeof(CurrentProfileAmplitudeData);

    std::vector<CurrentProfileCorrelationData> correlation_data(num_cells);
    std::memcpy(correlation_data.data(), data + data_offset,
                num_cells * sizeof(CurrentProfileCorrelationData));

    CurrentProfileDatagram datagram{};
    datagram.current_profle = current_profile_data;
    datagram.velocity_data = velocity_data;
    datagram.amplitude_data = amplitude_data;
    datagram.correlation_data = correlation_data;
    return datagram;
}

uint16_t calculate_checksum(const uint8_t *packet, size_t len) {
    uint16_t sum = 0xB58C;

    for (size_t i = 0; i < len; i += 2) {
        sum += (uint16_t) (packet[i] | (packet[i + 1] << 8));
    }

    if (len & 1) {
        sum += (uint16_t) (packet[len - 1] << 8);
    }

    return sum;
}


};  // namespace nortek::parser

NortekNucleusDriver::NortekNucleusDriver(

    asio::io_context& io,
    std::function<void(NortekNucleusFrame)> callback)
    : nucleus_sock_(io), callback_(callback) {}

std::error_code NortekNucleusDriver::open_tcp_sockets(
    const ConnectionParams& params) {
    std::error_code ec;
    auto addr = asio::ip::make_address(params.remote_ip, ec);
    if (ec) {
        return ec;
    }

    asio::ip::tcp::endpoint nucleus_endpoint(addr, params.data_remote_port);
    nucleus_sock_.connect(nucleus_endpoint, ec);
    if (ec) {
        return ec;
    }
    return {};
}

void NortekNucleusDriver::start_read(void) {
    nucleus_sock_.async_receive(
        asio::buffer(nucleus_buf_),
        std::bind(&NortekNucleusDriver::read_header, this,
                  std::placeholders::_1, std::placeholders::_2));
}

void NortekNucleusDriver::start_read_header() {
    asio::async_read(nucleus_sock_, asio::buffer(&header_, sizeof(header_)),
                     std::bind(&NortekNucleusDriver::read_header, this,
                               std::placeholders::_1, std::placeholders::_2));
}



void NortekNucleusDriver::read_header(const std::error_code error_code,
                                      std::size_t len) {
    if (len < sizeof(HeaderData) || error_code) {
        start_read_header();
        return;
    }

    if (header_.sync_byte != 0xA5) {
        start_read_header();
        return;
    }
    uint16_t actual_checksum = nortek::parser::calculate_checksum(reinterpret_cast<const uint8_t*>(&header_),
                        sizeof(HeaderData) - 1);

    if (actual_checksum != header_.header_checksum){
        start_read_header();
        return;
    }

    DataSeriesId id = static_cast<DataSeriesId>(header_.data_series_id);
    start_read_body(header_, header_.data_size);
}

void NortekNucleusDriver::start_read_body(const HeaderData header,
                                          std::size_t len) {
    asio::async_read(
        nucleus_sock_, asio::buffer(nucleus_buf_.data(), len),
        std::bind(&NortekNucleusDriver::read_body, this, std::placeholders::_1,
                  std::placeholders::_2, header));
}

void NortekNucleusDriver::read_body(const std::error_code& error_code,
                                    std::size_t len,
                                    const HeaderData header) {
    uint16_t checksum = nortek::parser::calculate_checksum(nucleus_buf_.data(), len);
    if (checksum != header.data_checksum) {
        start_read_header();
        return;
    }
    CommonData common_data_header =
        nortek::parser::read_from_buffer<CommonData>(
            nucleus_buf_.data(), nucleus_buf_.size(), 0);

    const std::size_t header_offset = sizeof(CommonData);
    const std::size_t data_offset = common_data_header.data_offset;
    const DataSeriesId id = static_cast<DataSeriesId>(header.data_series_id);

    switch (id) {
        case DataSeriesId::ImuData: {
            ImuData imu_data = nortek::parser::read_from_buffer<ImuData>(
                nucleus_buf_.data(), nucleus_buf_.size(), header_offset);
            callback_(imu_data);
            break;
        }
        case DataSeriesId::MagnometerData: {
            MagnetoMeterData magnometer_data =
                nortek::parser::read_from_buffer<MagnetoMeterData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), header_offset);
            callback_(magnometer_data);
            break;
        }
        case DataSeriesId::FieldCalibrationData: {
            FieldCalibrationData field_calibration_data =
                nortek::parser::read_from_buffer<FieldCalibrationData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), header_offset);
            callback_(field_calibration_data);
            break;
        }
        case DataSeriesId::FastPressureData: {
            FastPressureData fast_pressure_data =
                nortek::parser::read_from_buffer<FastPressureData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), data_offset);
            callback_(fast_pressure_data);
            break;
        }
        case DataSeriesId::AltimeterData: {
            AltimeterData altimeter_data =
                nortek::parser::read_from_buffer<AltimeterData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), header_offset);
            callback_(altimeter_data);
            break;
        }
        case DataSeriesId::BottomTrackData: {
            BottomTrackData bottom_track_data =
                nortek::parser::read_from_buffer<BottomTrackData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), header_offset);
            callback_(bottom_track_data);
            break;
        }
        case DataSeriesId::WaterTrackData: {
            WaterTrackData water_track_data =
                nortek::parser::read_from_buffer<WaterTrackData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), header_offset);
            callback_(water_track_data);
            break;
        }
        case DataSeriesId::CurrentProfileData: {
            CurrentProfileDatagram datagram =
                nortek::parser::parse_current_profile_data(
                    nucleus_buf_.data(), nucleus_buf_.size(), data_offset);
            callback_(datagram);
            break;
        }
        case DataSeriesId::SpectrumDataV3: {
            SpectrumDatagram spectrum_datagram =
                nortek::parser::parse_spectrum_data(nucleus_buf_.data(),
                                                           nucleus_buf_.size());
            callback_(spectrum_datagram);
            break;
        }
        case DataSeriesId::AhrsData: {
            AhrsDataV2 ahrs_data =
                nortek::parser::read_from_buffer<AhrsDataV2>(
                    nucleus_buf_.data(), nucleus_buf_.size(), header_offset);
            callback_(ahrs_data);
            break;
        }
        case DataSeriesId::InsData: {
            InsDataV2 ins_data =
                nortek::parser::read_from_buffer<InsDataV2>(
                    nucleus_buf_.data(), nucleus_buf_.size(), data_offset);
            callback_(ins_data);
            break;
        }
        case DataSeriesId::StringData: {
            const std::size_t data_size = len;
            std::string payload;
            payload.assign(reinterpret_cast<char*>(nucleus_buf_.data()),
                           data_size);
            callback_(payload);
            break;
        }
        default:
            break;
    }
    start_read_header();
}

NucleusReply NortekNucleusDriver::send_command(const std::string& cmd) {
    NucleusReply reply{};
    std::error_code error_code{};
    std::string msg = cmd + "\r\n";
    asio::write(nucleus_sock_, asio::buffer(msg), error_code);
    if (error_code) {
        reply.status = NucleusStatusCode::SendFailed;
        return reply;
    }

    asio::streambuf buf{};
    std::size_t len = asio::read_until(nucleus_sock_, buf, "\r\n", error_code);

    if (error_code) {
        reply.status = NucleusStatusCode::ReadFailed;
        return reply;
    }

    std::istream istream(&buf);
    std::string resp;
    std::getline(istream, resp);

    if (!resp.empty() && resp.back() == '\r')
        resp.pop_back();

    reply.status = NucleusStatusCode::Ok;
    reply.payload = resp;
    return reply;
}

NucleusStatusCode NortekNucleusDriver::start_nucleus() {
    const std::string cmd = "START";
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::stop_nucleus() {
    const std::string cmd = "STOP";
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::trigger_read() {
    const std::string cmd = "TRIG";
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::get_settings(const std::string& type) {
    std::string cmd = "GET" + type;
    return send_command(cmd).status;
}

NucleusReply NortekNucleusDriver::get_error() {
    std::string cmd = "GETERROR";
    return send_command(cmd);
}

NucleusStatusCode NortekNucleusDriver::set_bottom_track_settings(
    const BottomTrackSettings& settings) {
    std::string cmd = "SETBT,";

    switch (settings.mode) {
        case BottomTrackMode::FastACQ:
            cmd += "MODE=\"FAST_ACQ\"";
            break;
        case BottomTrackMode::Crawler:
            cmd += "MODE=\"CRAWLER\"";
            break;
        case BottomTrackMode::Auto:
            cmd += "MODE=\"AUTO\"";
            break;
    }
    cmd += ",";

    cmd += "VR=" + std::to_string(settings.velocity_range) + ",";

    if (settings.enable_watertrack) {
        cmd += "WT=\"ON\",";
    } else {
        cmd += "WT=\"OFF\",";
    }

    if (settings.power_level_user_defined) {
        cmd += "PLMODE=\"USER\",";
        cmd += "PL=" + std::to_string(settings.power_level) + ",";
    } else {
        cmd += "PLMODE=\"MAX\",";
    }

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case NucleusDataFormats::BottomTrackBinaryFormat:
            cmd += "DF=180,";
            break;
        case NucleusDataFormats::BottomTrackRDIPD6:
            cmd += "DF=156,";
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_altimeter_settings(
    const AltimeterSettings& settings) {
    std::string cmd = "SETALTI,";
    cmd += "PL=" + std::to_string(settings.power_level) + ",";

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case NucleusDataFormats::AltimeterFormat:
            cmd += "DF=170,";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_fast_pressure_settings(
    const FastPressureSettings& settings) {
    std::string cmd = "SETFASTPRESSURE,";

    if (settings.enable_fast_pressure) {
        cmd += "EN=1";
    } else {
        cmd += "EN=0";
    }

    cmd += "SR=" + std::to_string(settings.sampling_rate) + ",";

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case NucleusDataFormats::FastPressureFormat:
            cmd += "DF=150,";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_magnetometer_settings(
    const MagnetometerSettings& settings) {
    std::string cmd = "SETMAG,";

    cmd += "FREQ=" + std::to_string(settings.freq) + ",";

    switch (settings.mode) {
        case MagnetometerMethod::Off:
            cmd += "METHOD=\"OFF\",";
            break;
        case MagnetometerMethod::Auto:
            cmd += "METHOD=\"AUTO\",";
            break;
        case MagnetometerMethod::Wmm:
            cmd += "METHOD=\"WMM\",";
            break;
        default:
            break;
    }

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case DataSeriesId::MagnometerData:
            cmd += "DF=135,";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_ethernet_settings(
    const EthernetSettings& settings) {
    std::string cmd = "SETETH,";

    if (settings.use_static_ip) {
        cmd += "IPMETHOD=\"STATIC\",";
        cmd += "IP=\"" + settings.ip + "\",";
        cmd += "NETMASK=\"" + settings.netmask + "\",";
        cmd += "GATEWAY=\"" + settings.default_gateway + "\",";
    } else {
        cmd += "IPMETHOD=\"DHCP\",";
    }

    if (settings.use_password) {
        cmd += "PASSWORD=\"" + settings.password + "\",";
    } else {
        cmd += "PASSWORD=\"\"";
    }

    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::save_settings(
    const SaveSettings settings) {
    std::string cmd = "SAVE,";

    switch (settings) {
        case SaveSettings::All:
            cmd += "ALL";
            break;
        case SaveSettings::Config:
            cmd += "CONFIG";
            break;
        case SaveSettings::Comm:
            cmd += "COMM";
            break;
        case SaveSettings::Mission:
            cmd += "MISSION";
            break;
        case SaveSettings::Magcal:
            cmd += "MAGCAL";
            break;
        default:
            break;
    }

    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_ahrs_settings(
    const AhrsSettings& settings) {
    std::string cmd = "SETAHRS,";

    cmd += "FREQ=" + std::to_string(settings.freq) + ",";

    switch (settings.mode) {
        case AhrsMode::FixedHardAndSoftIron:
            cmd += "METHOD=0,";
            break;
        case AhrsMode::HardIronEstimation:
            cmd += "METHOD=1,";
            break;
        case AhrsMode::HardAndSoftEstimation:
            cmd += "METHOD=2,";
            break;
        default:
            break;
    }

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case DataSeriesId::AhrsData:
            cmd += "DF=210,";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}
