
#ifndef NORTEK_NUCLEUS_DRIVER_HPP_
#define NORTEK_NUCLEUS_DRIVER_HPP_

#include <asio.hpp>
#include <cstddef>
#include <functional>
#include <string>
#include <variant>
#include <vector>
#include "nortek_nucleus_messages.hpp"

struct ConnectionParams {
    std::string remote_ip;
    uint16_t data_remote_port;
    std::string password;
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

    void start_read();

    std::error_code enter_password(const ConnectionParams& params);

    /**
     * @brief Send command string to nucleus
     *
     * @param cmd Valid command string
     *
     * @return NucleusReply containing statuscode and payload
     */
    NucleusReply send_command(const std::string& cmd);

    /**
     * @brief Starts nucleus acoustic measurements
     *
     * Should be avoided when drytesting
     *
     * @return NucleusStatusCode indicating success or failure
     */
    NucleusStatusCode start_nucleus();

    /**
     * @brief Stops nucleus acoustic measurements
     *
     * @return NucleusStatusCode indicating success or failure
     */
    NucleusStatusCode stop_nucleus();
    /**
     * @brief Triggers and acoustic reading
     *
     * @return NucleusStatusCode indicating success or failure
     */
    NucleusStatusCode trigger_read();
    NucleusStatusCode get_settings(const std::string& type);
    /**
     * @brief Queries the nucleus for cause of error
     *
     * @return NucleusReply containing statuscode and eventually payload
     */
    NucleusReply get_error();

    /**
     * @brief Sets bottomtrack settings according to values specifeid in
     * BottomTrackSettings struct
     *
     * @param settings Struct containing BottomTrackSettings
     *
     * @return A statuscode indicating success or failure
     */
    NucleusStatusCode set_bottom_track_settings(
        const BottomTrackSettings& settings);
    /**
     * @brief Sets Altimeter settings according to values specifeid in
     * AltimeterSettings struct
     *
     * @param settings Struct containing Altimeter Settings
     *
     * @return A statuscode indicating success or failure
     */
    NucleusStatusCode set_altimeter_settings(const AltimeterSettings& settings);
    /**
     * @brief Sets FastPressre settings according to values specifeid in
     * FastPressureSettings struct
     *
     * @param settings Struct containing FastPressureSettings
     *
     * @return A statuscode indicating success or failure
     */
    NucleusStatusCode set_fast_pressure_settings(
        const FastPressureSettings& settings);
    /**
     * @brief Sets magnetometer settings according to values specifeid in
     * MagnetometerSettings struct
     *
     * @param settings Struct containing MagnetometerSettings
     *
     * @return A statuscode indicating success or failure
     */
    NucleusStatusCode set_magnetometer_settings(
        const MagnetometerSettings& settings);
    /**
     * @brief Sets ethernet settings according to values specifeid in
     * EthernetSettings struct
     *
     * @param settings Struct containing EthernetSettings
     *
     * @return A statuscode indicating success or failure
     */
    NucleusStatusCode set_ethernet_settings(const EthernetSettings& settings);
    /**
     * @brief Sets SaveSettings settings according to values specifeid in
     * SaveSettings struct
     *
     * @param settings Struct containing SaveSettings
     *
     * @return A statuscode indicating success or failure
     */
    NucleusStatusCode save_settings(const SaveSettings settings);
    /**
     * @brief Sets Ahrs settings according to values specifeid in
     * AhrsSettings struct
     *
     * @param settings Struct containing AhrsSettings
     *
     * @return A statuscode indicating success or failure
     */
    NucleusStatusCode set_ahrs_settings(const AhrsSettings& settings);

   private:
    void parse_available();
    void dispatch(const uint8_t* payload,
                  size_t payload_size,
                  const HeaderData& header);

    std::vector<uint8_t> buf;
    std::array<uint8_t, 4096> temp;
    std::size_t read_index = 0;
    asio::ip::tcp::socket nucleus_sock_;
    std::function<void(NortekNucleusFrame)> callback_;
};

#endif  // NORTEK_NUCLEUS_DRIVER_HPP_
