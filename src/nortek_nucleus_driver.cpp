
#include "nortek_nucleus_driver.hpp"
#include <sys/stat.h>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include "nortek_nucleus_messages.hpp"

NortekNucleusDriver::NortekNucleusDriver(

    asio::io_context& io, std::function<void(NortekNucleusFrame)> callback)
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
        std::bind(&NortekNucleusDriver::read_data, this, std::placeholders::_1,
                  std::placeholders::_2));
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

void NortekNucleusDriver::read_data(const std::error_code& error_code,
                                    std::size_t len) {
    std::size_t offset = 0;
    HeaderData header = read_from_buffer<HeaderData>(
        nucleus_buf_.data(), nucleus_buf_.size(), offset);
    offset += sizeof(HeaderData);

    DataSeriesId data_series = static_cast<DataSeriesId>(header.data_series_id);

    CommonData common_data_header = read_from_buffer<CommonData>(
        nucleus_buf_.data(), nucleus_buf_.size(), offset);
    offset += sizeof(CommonData);

    switch (data_series) {
        case DataSeriesId::ImuData: {
            ImuData imu_data = read_from_buffer<ImuData>(
                nucleus_buf_.data(), nucleus_buf_.size(), offset);
            break;
        }
        case DataSeriesId::MagnometerData: {
            MagnetoMeterData magnometer_data =
                read_from_buffer<MagnetoMeterData>(nucleus_buf_.data(),
                                                   nucleus_buf_.size(), offset);
            break;
        }
        case DataSeriesId::FieldCalibrationData: {
            FieldCalibrationData field_calibration_data =
                read_from_buffer<FieldCalibrationData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), offset);
            break;
        }
        case DataSeriesId::FastPressureData: {
            const std::size_t data_offset =
                sizeof(HeaderData) + common_data_header.data_offset;

            FastPressureData fast_pressure_data =
                read_from_buffer<FastPressureData>(
                    nucleus_buf_.data(), nucleus_buf_.size(), data_offset);
            break;
        }
        case DataSeriesId::StringData: {
            const std::size_t data_size = header.data_size;
            constexpr std::size_t header_offset = sizeof(HeaderData);
            std::string payload;
            payload.assign(
                reinterpret_cast<char*>(nucleus_buf_.data() + header_offset),
                data_size);
            break;
        }
        case DataSeriesId::AltimeterData: {
            AltimeterData altimeter_data = read_from_buffer<AltimeterData>(
                nucleus_buf_.data(), nucleus_buf_.size(), offset);
            break;
        }
        case DataSeriesId::BottomTrackData: {
            BottomTrackData bottom_track_data =
                read_from_buffer<BottomTrackData>(nucleus_buf_.data(),
                                                  nucleus_buf_.size(), offset);
            break;
        }
        case DataSeriesId::WaterTrackData: {
            WaterTrackData water_track_data = read_from_buffer<WaterTrackData>(
                nucleus_buf_.data(), nucleus_buf_.size(), offset);
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
}

std::error_code NortekNucleusDriver::send_command(const std::string& cmd) {
    std::error_code error_code{};
    std::string msg = cmd + "\r\n";
    asio::write(nucleus_sock_, asio::buffer(msg), error_code);

    asio::streambuf buf{};
    asio::read_until(nucleus_sock_, buf, "\r\n", error_code);
    if (error_code)
        return error_code;

    std::istream istream(&buf);
    std::string resp;
    std::getline(istream, resp);

    if (!resp.empty() && resp.back() == '\r')
        resp.pop_back();

    if (resp != "OK") {
        return std::make_error_code(std::errc::protocol_error);
    }

    return {};
}
