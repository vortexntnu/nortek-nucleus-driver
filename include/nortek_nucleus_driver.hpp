
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

struct StreamState {
    std::vector<uint8_t> buf;
    std::array<uint8_t, 4096> temp;
};

using NortekNucleusFrame = std::variant<ImuData,
                                        MagnetoMeterData,
                                        FieldCalibrationData,
                                        FastPressureData,
                                        BottomTrackData,
                                        WaterTrackData,
                                        CurrentProfileDatagram,
                                        AltimeterData,
                                        AhrsDataV2,
                                        InsDataV2,
                                        SpectrumDatagram,
                                        std::string>;

class NortekNucleusDriver {
   public:
    explicit NortekNucleusDriver(asio::io_context& io,
                                 std::function<void(NortekNucleusFrame)>);

    std::error_code open_tcp_sockets(const ConnectionParams& params);

    void start_read(StreamState& st, asio::ip::tcp::socket& sock);

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
    NucleusStatusCode save_settings(const SaveSettings settings);
    NucleusStatusCode set_ahrs_settings(const AhrsSettings& settings);

   private:
    void find_sync_byte(StreamState& st, asio::ip::tcp::socket& sock);
    void parse_available(StreamState& state);

    asio::ip::tcp::socket nucleus_sock_;
    std::array<uint8_t, 4096> nucleus_buf_;
    std::function<void(NortekNucleusFrame)> callback_;
};

#endif  // NORTEK_NUCLEUS_DRIVER_HPP_
