#pragma once

#include "../std.hpp"
#include "../ip/sockets.hpp"
#include "../util/noncopyable.hpp"
#include "eventloop.hpp"
#include "connection.hpp"

namespace cortono::net
{
    class TcpClient : private util::noncopyable
    {
        public:
            static TcpConnection::Pointer connect(EventLoop* loop,
                                                  const std::string& ip,
                                                  unsigned short port,
                                                  std::function<void(TcpConnection::Pointer)> read_cb = nullptr,
                                                  std::function<void(TcpConnection::Pointer)> write_cb = nullptr,
                                                  std::function<void(TcpConnection::Pointer)> close_cb = nullptr) {
                std::string ip_address = ip::address::parse_ip_address(ip);
                log_info("start to connect to server:", ip_address, port);
                int fd = ip::tcp::sockets::block_socket();
                if(!ip::tcp::sockets::connect(fd, ip_address, port)) {
                    log_error("fail to connect to server...", ip, port, std::strerror(errno));
                    return nullptr;
                }
#ifdef CORTONO_USE_SSL
                SSL* ssl = ip::tcp::ssl::new_ssl_and_set_fd(fd);
                ip::tcp::ssl::connect(ssl);
                auto conn_ptr = std::make_shared<TcpConnection>(loop, fd, ssl);
#else
                auto conn_ptr = std::make_shared<TcpConnection>(loop, fd);
#endif
                conn_ptr->on_read(std::move(read_cb));
                conn_ptr->on_write(std::move(write_cb));
                conn_ptr->on_close(std::move(close_cb));
                return conn_ptr;
            }
    };
}
