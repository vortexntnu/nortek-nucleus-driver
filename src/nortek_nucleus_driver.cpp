
#include "nortek_nucleus_driver.hpp"
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include "nortek_nucleus_messages.hpp"

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

bool verify_checksum(const uint8_t* data, std::size_t len, uint16_t checksum) {
    uint16_t true_checksum = 0xB58C;

    size_t i = 0;
    while (i < len) {
        uint16_t u = data[i];

        uint16_t v = 0x00;

        if (i + 1 < len)
            v = data[i + 1];

        uint16_t word = static_cast<uint16_t>(u | (v << 8));

        true_checksum = static_cast<uint16_t>((true_checksum + word) & 0xFFFF);

        i += 2;
    }
    return true_checksum == checksum;
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
    if (verify_checksum(reinterpret_cast<const uint8_t*>(&header_),
                        sizeof(HeaderData) - 1, header_.header_checksum)) {
        start_read_header();
        return;
    }

    DataSeriesId id = static_cast<DataSeriesId>(header_.data_series_id);
    start_read_body(id, header_.data_size);
}

void NortekNucleusDriver::start_read_body(DataSeriesId id, std::size_t len) {
    asio::async_read(nucleus_sock_, asio::buffer(nucleus_buf_.data(), len),
                     std::bind(&NortekNucleusDriver::read_data, this,
                               std::placeholders::_1, std::placeholders::_2, id));
}

void NortekNucleusDriver::read_data(const std::error_code& error_code,
                                    std::size_t len, const DataSeriesId id) {
    std::size_t offset = 0;
    CommonData common_data_header = read_from_buffer<CommonData>(
        nucleus_buf_.data(), nucleus_buf_.size(), offset);
    offset += sizeof(CommonData);

    switch (id) {
        case DataSeriesId::ImuData: {
            ImuData imu_data = read_from_buffer<ImuData>(
                nucleus_buf_.data(), nucleus_buf_.size(), offset);
            callback_(imu_data);
            break;
        }
        case DataSeriesId::MagnometerData: {
            MagnetoMeterData magnometer_data =
                read_from_buffer<MagnetoMeterData>(nucleus_buf_.data(),
                                                   nucleus_buf_.size(), offset);
            callback_(magnometer_data);
            break;
        }
        case DataSeriesId::FieldCalibrationData: {
            FieldCalibrationData field_calibration_data =
                read_from_buffer<FieldCalibrationData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), offset);
            callback_(field_calibration_data);
            break;
        }
        case DataSeriesId::FastPressureData: {
            const std::size_t data_offset =
                sizeof(HeaderData) + common_data_header.data_offset;

            FastPressureData fast_pressure_data =
                read_from_buffer<FastPressureData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), data_offset);
            callback_(fast_pressure_data);
            break;
        }
        case DataSeriesId::StringData: {
            const std::size_t data_size = len;
            constexpr std::size_t header_offset = sizeof(HeaderData);
            std::string payload;
            payload.assign(
                reinterpret_cast<char*>(nucleus_buf_.data() + header_offset),
                data_size);
            callback_(payload);
            break;
        }
        case DataSeriesId::AltimeterData: {
            AltimeterData altimeter_data = read_from_buffer<AltimeterData>(
                nucleus_buf_.data(), nucleus_buf_.size(), offset);
            callback_(altimeter_data);
            break;
        }
        case DataSeriesId::BottomTrackData: {
            BottomTrackData bottom_track_data =
                read_from_buffer<BottomTrackData>(nucleus_buf_.data(),
                                                  nucleus_buf_.size(), offset);
            callback_(bottom_track_data);
            break;
        }
        case DataSeriesId::WaterTrackData: {
            WaterTrackData water_track_data = read_from_buffer<WaterTrackData>(
                nucleus_buf_.data(), nucleus_buf_.size(), offset);
            callback_(water_track_data);
            break;
        }
        case DataSeriesId::CurrentProfileData: {
            CurrentProfileData current_profile_data =
                read_from_buffer<CurrentProfileData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), offset);

            const uint16_t num_cells = current_profile_data.num_cells;
            const std::size_t data_offset =
                sizeof(HeaderData) + common_data_header.data_offset;

            std::vector<CurrentProfileVelocityData> velocity_data(num_cells);
            std::memcpy(velocity_data.data(), nucleus_buf_.data() + data_offset,
                        num_cells * sizeof(CurrentProfileVelocityData));

            break;
        }
        case DataSeriesId::AhrsData: {
            AhrsDataV2 ahrs_data = read_from_buffer<AhrsDataV2>(
                nucleus_buf_.data(), nucleus_buf_.size(), offset);
            callback_(ahrs_data);
            break;
        }
        case DataSeriesId::InsData: {
            const std::size_t data_offset =
                sizeof(HeaderData) + common_data_header.data_offset;

            InsDataV2 ins_data = read_from_buffer<InsDataV2>(
                nucleus_buf_.data(), nucleus_buf_.size(), data_offset);
            break;
        }
        case DataSeriesId::SpectrumDataV3: {
            constexpr std::size_t header_offset = sizeof(HeaderData);
            SpectrumDataV3 spectrum_data = read_from_buffer<SpectrumDataV3>(
                nucleus_buf_.data(), nucleus_buf_.size(), header_offset);

            const std::size_t data_offset =
                sizeof(HeaderData) + spectrum_data.data_offset;

            const uint16_t bins = spectrum_data.num_beam_bins & 0x1FFF;
            const uint8_t beams = spectrum_data.num_beam_bins >> 13;

            SpectrumHeader spectrum_freq_header =
                read_from_buffer<SpectrumHeader>(nucleus_buf_.data(),
                                                 nucleus_buf_.size(),
                                                 header_offset + data_offset);

            std::vector<int16_t> spectrum_freq_data(bins * beams);
            const std::size_t num_bytes = sizeof(int16_t) * bins * beams;
            std::memcpy(spectrum_freq_data.data(),
                        nucleus_buf_.data() + header_offset + data_offset + 64,
                        num_bytes);

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
