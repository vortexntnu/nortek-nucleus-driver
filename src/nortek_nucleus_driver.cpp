
#include "nortek_nucleus_driver.hpp"
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <cstring>
#include "nortek_nucleus_messages.hpp"

NortekNucleusDriver::NortekNucleusDriver(

    asio::io_context& io)
    : nucleus_sock_(io) {}

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

void NortekNucleusDriver::read_data(const std::error_code& error_code,
                                    std::size_t len) {
    HeaderData header{};
    std::memcpy(&header, nucleus_buf_.data(), sizeof(HeaderData));

    DataSeriesId data_series = static_cast<DataSeriesId>(header.data_series_id);
    switch (data_series) {
        case DataSeriesId::ImuData: {
            CommonData common_data_header{};
            std::memcpy(&common_data_header,
                        nucleus_buf_.data() + sizeof(HeaderData),
                        sizeof(common_data_header));

            ImuData imu_data{};
            std::memcpy(&imu_data, nucleus_buf_.data(), sizeof(ImuData));
            break;
        }
        case DataSeriesId::MagnometerData: {
            CommonData common_data_header{};
            std::memcpy(&common_data_header,
                        nucleus_buf_.data() + sizeof(HeaderData),
                        sizeof(common_data_header));

            MagnetoMeterData magnometer_data{};
            std::memcpy(&magnometer_data, nucleus_buf_.data(),
                        sizeof(MagnetoMeterData));
            break;
        }
        case DataSeriesId::FieldCalibrationData: {
            CommonData common_data_header{};
            std::memcpy(&common_data_header,
                        nucleus_buf_.data() + sizeof(HeaderData),
                        sizeof(common_data_header));

            FieldCalibrationData field_calibration_data{};
            std::memcpy(&field_calibration_data, nucleus_buf_.data(),
                        sizeof(FieldCalibrationData));
            break;
        }
        case DataSeriesId::FastPressureData: {
            CommonData common_data_header{};
            std::memcpy(&common_data_header,
                        nucleus_buf_.data() + sizeof(HeaderData),
                        sizeof(common_data_header));

            FastPressureData fast_pressure_data{};
            std::memcpy(&fast_pressure_data, nucleus_buf_.data(),
                        sizeof(FastPressureData));
            break;
        }
        case DataSeriesId::StringData: {
            break;
        }
        case DataSeriesId::AltimeterData: {
            CommonData common_data_header{};
            std::memcpy(&common_data_header,
                        nucleus_buf_.data() + sizeof(HeaderData),
                        sizeof(common_data_header));

            AltimeterData altimeter_data{};
            std::memcpy(&altimeter_data, nucleus_buf_.data(),
                        sizeof(AltimeterData));
            break;
        }
        case DataSeriesId::BottomTrackData: {
            break;
        }
        case DataSeriesId::WaterTrackData:
            break;
        case DataSeriesId::CurrentProfileData:
            break;
        case DataSeriesId::AhrsData: {
            CommonData common_data_header{};
            std::memcpy(&common_data_header,
                        nucleus_buf_.data() + sizeof(HeaderData),
                        sizeof(common_data_header));

            AhrsDataV2 ahrs_data{};
            std::memcpy(&ahrs_data, nucleus_buf_.data(), sizeof(AhrsDataV2));
            break;
        }
        case DataSeriesId::InsData: {
            CommonData common_data_header{};
            std::memcpy(&common_data_header,
                        nucleus_buf_.data() + sizeof(HeaderData),
                        sizeof(common_data_header));

            InsDataV2 ins_data_v2{};
            std::memcpy(&ins_data_v2, nucleus_buf_.data(), sizeof(InsDataV2));
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
