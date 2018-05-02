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
            using ClientConnType = TcpConnection;

            static typename TcpConnection::Pointer connect(EventLoop* loop,
                                                           const std::string& ip,
                                                           unsigned short port,
                                                           std::function<void(TcpConnection::Pointer)> read_cb = nullptr,
                                                           std::function<void(TcpConnection::Pointer)> write_cb = nullptr,
                                                           std::function<void(TcpConnection::Pointer)> close_cb = nullptr) {
                std::string ip_address = ip::address::parse_ip_address(ip);
                log_info("start to connect to server:", ip_address, port);
                int fd = ip::tcp::sockets::nonblock_socket();
                bool connected = false;
                if(!ip::tcp::sockets::connect(fd, ip_address, port)) {
                    // errno == EINPROGRESS
                    // FIXME: 多线程下errno的安全性
                    if(!ip::tcp::sockets::is_connecting()) {
                        log_error("fail to connect to server...", ip, port, std::strerror(errno));
                        return nullptr;
                    }
                    log_info("connecting to server socket");
                }
                else {
                    connected = true;
                    log_info("connect to server socket done");
                }
                auto conn_ptr = std::make_shared<TcpConnection>(loop, fd);
                if(!connected) {
                    conn_ptr->set_conn_state(ClientConnType::ConnState::HandShaking);
                }
                else {
                    conn_ptr->set_conn_state(ClientConnType::ConnState::Connected);
                }
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
            using ClientConnType = SslConnection;

            static typename SslConnection::Pointer connect(EventLoop* loop,
                                                           const std::string& ip,
                                                           unsigned short port,
                                                           std::function<void(SslConnection::Pointer)> read_cb = nullptr,
                                                           std::function<void(SslConnection::Pointer)> write_cb = nullptr,
                                                           std::function<void(SslConnection::Pointer)> close_cb = nullptr) {
                static bool inited = false;
                if(!inited) {
                    ip::tcp::ssl::init_ssl();
                    inited = true;
                }
                ip::tcp::ssl::load_certificate(CA_CERT_FILE, CLIENT_CERT_FILE, CLIENT_KEY_FILE, true, false);
                std::string ip_address = ip::address::parse_ip_address(ip);
                log_info("start to connect to server:", ip_address, port);
                int fd = ip::tcp::sockets::nonblock_socket();
                bool connected = false;
                if(!ip::tcp::sockets::connect(fd, ip_address, port)) {
                    if(errno != EINPROGRESS) {
                        log_error("fail to connect to server...", ip, port, std::strerror(errno));
                        ip::tcp::sockets::close(fd);
                        return nullptr;
                    }
                    log_info("connecting to server socket");
                }
                else {
                    connected = true;
                    log_info("connect to server socket done");
                }
                SSL* ssl = ip::tcp::ssl::new_ssl_and_set_fd(fd);
                auto conn_ptr = std::make_shared<SslConnection>(loop, fd, ssl);
                if(!connected) {
                    conn_ptr->set_conn_state(ClientConnType::ConnState::HandShaking);
                }
                else {
                    if(ip::tcp::ssl::connect(ssl)) {
                        log_info("connect done");
                        conn_ptr->set_conn_state(ClientConnType::ConnState::Connected);
                    }
                    // SSL_get_errors返回SSL_ERROR_WANT_READ/SSL_ERROR_WANT_READ，代表正在握手
                    // FIXME
                    else if(ip::tcp::ssl::is_connecting(ssl)) {
                        log_info("connecting");
                        conn_ptr->set_conn_state(ClientConnType::ConnState::HandShaking);
                    }
                    else {
                        log_info("connect error");
                        conn_ptr->close();
                        return nullptr;
                    }
                }
                conn_ptr->on_read(std::move(read_cb));
                conn_ptr->on_write(std::move(write_cb));
                conn_ptr->on_close(std::move(close_cb));
                return conn_ptr;
            }
    };
#endif
}
