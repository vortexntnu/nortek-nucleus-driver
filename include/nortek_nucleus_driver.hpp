
#ifndef NORTEK_NUCLEUS_DRIVER_HPP_
#define NORTEK_NUCLEUS_DRIVER_HPP_

#include <asio.hpp>
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
                                        AhrsDataV2,
                                        InsDataV2,
                                        SpectrumDataV3>;

class NortekNucleusDriver {
   public:
    explicit NortekNucleusDriver(asio::io_context& io,
                                 std::function<void(NortekNucleusFrame)>);

    std::error_code open_tcp_sockets(const ConnectionParams& params);

    void start_read(void);

    std::error_code send_command(const std::string& cmd);

   private:
    void read_data(const std::error_code& error_code, std::size_t n);

    asio::ip::tcp::socket nucleus_sock_;
    std::array<uint8_t, 1500> nucleus_buf_;

    std::function<void(NortekNucleusFrame)> callback_;
};

#endif  // NORTEK_NUCLEUS_DRIVER_HPP_
