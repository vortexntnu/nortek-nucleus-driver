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

uint16_t calculate_checksum(const uint8_t* packet, size_t len) {
    uint16_t sum = 0xB58C;

    for (size_t i = 0; i < len; i += 2) {
        sum += (uint16_t)(packet[i] | (packet[i + 1] << 8));
    }

    if (len & 1) {
        sum += (uint16_t)(packet[len - 1] << 8);
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

void NortekNucleusDriver::start_read(StreamState& st,
                                     asio::ip::tcp::socket& sock) {
    sock.async_read_some(asio::buffer(st.temp), [this, &st, &sock](
                                                    std::error_code ec,
                                                    std::size_t size) {
        if (ec)
            return;
        st.buf.insert(st.buf.end(), st.temp.begin(), st.temp.begin() + size);

        parse_available(st);
        start_read(st, sock);
    });
}

void NortekNucleusDriver::parse_available(StreamState& st) {
    constexpr size_t MAX_FRAME = 1500;  // placeholder for now
    constexpr uint8_t SYNC_BYTE = 0xA5;

    auto it = std::find(st.buf.begin(), st.buf.end(), SYNC_BYTE);

    if (it == st.buf.end()) {
        if (st.buf.size() > 1) {
            st.buf.erase(st.buf.begin(), st.buf.end() - 1);  // keep last byte
        }
        return;
    }

    if (it != st.buf.begin()) {
        st.buf.erase(st.buf.begin(), it);
        it = st.buf.begin();
    }

    const size_t avail = st.buf.size();
    if (avail < sizeof(HeaderData)) {
        return;
    }

    HeaderData header = nortek::parser::read_from_buffer<HeaderData>(
        st.buf.data(), st.buf.size(), 0);

    if (header.sync_byte != SYNC_BYTE) {
        st.buf.erase(st.buf.begin());
        return;
    }

    uint16_t actual_checksum = nortek::parser::calculate_checksum(
        st.buf.data(), sizeof(HeaderData) - 1);

    if (actual_checksum != header.header_checksum) {
        st.buf.erase(st.buf.begin());
        return;
    }


    if (header.data_size > MAX_FRAME) {
        st.buf.erase(st.buf.begin());
    }

    const size_t frame_size = sizeof(HeaderData) + header.data_size;

    if (avail < frame_size) {
        return;
    }

    const uint8_t* payload = st.buf.data() + sizeof(HeaderData);
    const size_t payload_size = header.data_size;

    uint16_t data_checksum = nortek::parser::calculate_checksum(payload, payload_size);

    if (data_checksum != header.data_checksum){
        st.buf.erase(st.buf.begin());
        return;
    }

    CommonData common_data_header =
        nortek::parser::read_from_buffer<CommonData>(payload, payload_size, 0);

    const std::size_t header_offset = sizeof(CommonData);
    const std::size_t data_offset = common_data_header.data_offset;
    const DataSeriesId id = static_cast<DataSeriesId>(header.data_series_id);

    switch (id) {
        case DataSeriesId::ImuData: {
            callback_(nortek::parser::read_from_buffer<ImuData>(payload, payload_size,
                                                                header_offset));
            break;
        }
        case DataSeriesId::MagnometerData: {
            callback_(nortek::parser::read_from_buffer<MagnetoMeterData>(
                payload, payload_size, header_offset));
            break;
        }
        case DataSeriesId::FieldCalibrationData: {
            callback_(nortek::parser::read_from_buffer<FieldCalibrationData>(
                payload, payload_size, header_offset));
            break;
        }
        case DataSeriesId::FastPressureData: {
            callback_(nortek::parser::read_from_buffer<FastPressureData>(
                payload, payload_size, data_offset));
            break;
        }
        case DataSeriesId::AltimeterData: {
            callback_(nortek::parser::read_from_buffer<AltimeterData>(
                payload, payload_size, header_offset));
            break;
        }
        case DataSeriesId::BottomTrackData: {
            callback_(nortek::parser::read_from_buffer<BottomTrackData>(
                payload, payload_size, header_offset));
            break;
        }
        case DataSeriesId::WaterTrackData: {
            callback_(nortek::parser::read_from_buffer<WaterTrackData>(
                payload, payload_size, header_offset));
            break;
        }
        case DataSeriesId::CurrentProfileData: {
            callback_(nortek::parser::parse_current_profile_data(
                payload, payload_size, data_offset));
            break;
        }
        case DataSeriesId::SpectrumDataV3: {
            callback_(nortek::parser::parse_spectrum_data(payload, payload_size));
            break;
        }
        case DataSeriesId::AhrsData: {
            callback_(nortek::parser::read_from_buffer<AhrsDataV2>(
                payload, payload_size, header_offset));
            break;
        }
        case DataSeriesId::InsData: {
            callback_(nortek::parser::read_from_buffer<InsDataV2>(
                payload, payload_size, data_offset));
            break;
        }
        case DataSeriesId::StringData: {
            std::string data_string;
            data_string.resize(payload_size);
            std::memcpy(data_string.data(), payload, payload_size);
            callback_(data_string);
            break;
        }
        default:
            break;
    }
    st.buf.erase(st.buf.begin(), st.buf.begin() + header.data_size);
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
