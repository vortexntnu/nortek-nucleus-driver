
#ifndef NORTEK_NUCLEUS_DRIVER_HPP_
#define NORTEK_NUCLEUS_DRIVER_HPP_

#include <asio.hpp>

struct ConnectionParams {
    std::string remote_ip;
    uint16_t data_remote_port;
    uint16_t data_local_port;
};

class NortekNucleusDriver {
   public:
    explicit NortekNucleusDriver(asio::io_context& io);

    std::error_code open_tcp_sockets(const ConnectionParams& params);




   private:
    asio::ip::tcp::socket nucleus_sock_;
};

#endif  // NORTEK_NUCLEUS_DRIVER_HPP_
