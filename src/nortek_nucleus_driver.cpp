
#include "nortek_nucleus_driver.hpp"
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
        case DataSeriesId::ImuData:
            break;
        case DataSeriesId::MagnometerData:
            break;
        case DataSeriesId::FieldCalibrationData:
            break;
        case DataSeriesId::FastPressureData:
            break;
        case DataSeriesId::StringData:
            break;
        case DataSeriesId::AltimeterData:
            break;
        case DataSeriesId::BottomTrackData:
            break;
        case DataSeriesId::WaterTrackData:
            break;
        case DataSeriesId::CurrentProfileData:
            break;
        case DataSeriesId::AhrsData:
            break;
        case DataSeriesId::InsData:
            break;
        default:
            break;
    }
}
