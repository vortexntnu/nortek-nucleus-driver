
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

    void start_read(void);

   private:
    void read_data(const std::error_code& error_code, std::size_t n);
    asio::ip::tcp::socket nucleus_sock_;
    std::array<uint8_t, 1500> nucleus_buf_;
};

#endif  // NORTEK_NUCLEUS_DRIVER_HPP_
