
#ifndef NORTEK_NUCLEUS_DRIVER_HPP_
#define NORTEK_NUCLEUS_DRIVER_HPP_

#include <asio.hpp>
#include <cstddef>
#include <functional>
#include <variant>
#include "nortek_nucleus_messages.hpp"

struct ConnectionParams {
    std::string remote_ip;
    uint16_t data_remote_port;
    uint16_t data_local_port;
};

using NortekNucleusFrame = std::variant<ImuData,
                                        MagnetoMeterData,
                                        FieldCalibrationData,
                                        FastPressureData,
                                        BottomTrackData,
                                        WaterTrackData,
                                        CurrentProfileData,
                                        AltimeterData,
                                        AhrsDataV2,
                                        InsDataV2,
                                        SpectrumDataV3,
                                        std::string>;

class NortekNucleusDriver {
   public:
    explicit NortekNucleusDriver(asio::io_context& io,
                                 std::function<void(NortekNucleusFrame)>);

    std::error_code open_tcp_sockets(const ConnectionParams& params);

    void start_read(void);
    void start_read_header();
    void start_read_body(const DataSeriesId id, std::size_t len);

    NucleusReply send_command(const std::string& cmd);

    NucleusStatusCode start_nucleus();
    NucleusStatusCode stop_nucleus();
    NucleusStatusCode trigger_read();
    NucleusStatusCode get_settings(const std::string& type);
    NucleusReply get_error();

    NucleusStatusCode set_bottom_track_settings(
        const BottomTrackSettings& settings);
    NucleusStatusCode set_altimeter_settings(const AltimeterSettings& settings);
    NucleusStatusCode set_fast_pressure_settings(
        const FastPressureSettings& settings);
    NucleusStatusCode set_magnetometer_settings(
        const MagnetometerSettings& settings);
    NucleusStatusCode set_ethernet_settings(const EthernetSettings& settings);

   private:
    void read_header(const std::error_code error_code, std::size_t len);
    void read_data(const std::error_code& error_code,
                   std::size_t n,
                   const DataSeriesId id);

    asio::ip::tcp::socket nucleus_sock_;
    std::array<uint8_t, 1500> nucleus_buf_;

    HeaderData header_{};
    std::function<void(NortekNucleusFrame)> callback_;
};

#endif  // NORTEK_NUCLEUS_DRIVER_HPP_
