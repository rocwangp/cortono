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
            static typename TcpConnection::Pointer connect(EventLoop* loop,
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
                log_info("connect to server socket done");
                auto conn_ptr = std::make_shared<TcpConnection>(loop, fd);
                ip::tcp::sockets::set_nonblock(fd);
                conn_ptr->on_read(std::move(read_cb));
                conn_ptr->on_write(std::move(write_cb));
                conn_ptr->on_close(std::move(close_cb));
                return conn_ptr;
            }
    };

#ifdef CORTONO_USE_SSL
    class SslClient : private util::noncopyable
    {
        public:
            static typename SslConnection::Pointer connect(EventLoop* loop,
                                                           const std::string& ip,
                                                           unsigned short port,
                                                           std::function<void(SslConnection::Pointer)> read_cb = nullptr,
                                                           std::function<void(SslConnection::Pointer)> write_cb = nullptr,
                                                           std::function<void(SslConnection::Pointer)> close_cb = nullptr) {
                static bool inited = false;
                if(!inited) {
                    ip::tcp::ssl::init_ssl();
                    ip::tcp::ssl::load_certificate(CA_CERT_FILE, CLIENT_CERT_FILE, CLIENT_KEY_FILE, true, false);
                    inited = true;
                }
                std::string ip_address = ip::address::parse_ip_address(ip);
                log_info("start to connect to server:", ip_address, port);
                int fd = ip::tcp::sockets::block_socket();
                if(!ip::tcp::sockets::connect(fd, ip_address, port)) {
                    log_error("fail to connect to server...", ip, port, std::strerror(errno));
                    return nullptr;
                }
                log_info("connect to server socket done");
                SSL* ssl = ip::tcp::ssl::new_ssl_and_set_fd(fd);
                log_info("connect to server ssl");
                ip::tcp::ssl::connect(ssl);
                log_info("connect to server ssl done");
                auto conn_ptr = std::make_shared<SslConnection>(loop, fd, ssl);
                ip::tcp::sockets::set_nonblock(fd);
                conn_ptr->on_read(std::move(read_cb));
                conn_ptr->on_write(std::move(write_cb));
                conn_ptr->on_close(std::move(close_cb));
                return conn_ptr;
            }
    };
#endif
}
