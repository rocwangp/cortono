#pragma once

#include "../cortono.hpp"
#include "../std.hpp"
#include "http_parser.hpp"
#include "http_response.hpp"

namespace cortono::http
{
    template <typename Handler>
    class HttpProxyConnection;

    template <typename Handler>
    class HttpProxyServer
    {
        public:
            HttpProxyServer(Handler& handler, const std::string& ip, unsigned short port, std::size_t concurrency)
                : handler_(handler),
                  service_(&loop_, ip, port),
                  concurrency_(concurrency)
            {
                init_callback();
            }

            void run() {
                service_.start(concurrency_);
                loop_.loop();
            }
        private:
            void init_callback() {
                service_.on_conn([&](auto conn_ptr) {
                    auto proxy_conn = std::make_shared<HttpProxyConnection<Handler>>(handler_);
                    conn_ptr->on_read([proxy_conn](auto conn_ptr) {
                        log_trace;
                        proxy_conn->handle_read(conn_ptr);
                    });
                });
            }
        private:
            Handler& handler_;
            std::size_t concurrency_{ 1 };
            net::EventLoop loop_;
            net::TcpService service_;
    };

    template <typename Handler>
    class HttpProxyConnection
    {
        public:
            HttpProxyConnection(Handler& handler)
                : handler_(handler)
            {  }

            void handle_read(net::TcpConnection::Pointer conn_ptr) {
                int len = parser_.feed(conn_ptr->recv_buffer()->data() + parse_len_, conn_ptr->recv_buffer()->size() - parse_len_);
                parse_len_ += len;
                if(parser_.done()) {
                    req_ = std::move(parser_.to_request());
                    std::string ip;
                    unsigned short port{ 0 };
                    if(req_.has_header("Host")) {
                        ip = req_.get_header_value("Host");
                        if(auto pos = ip.find_first_of(':'); pos != std::string::npos) {
                            port = std::strtoul(ip.substr(pos + 1).data(), nullptr, 10);
                            ip.resize(pos);
                        }
                        else {
                            port = 80;
                        }
                    }
                    else {
                        std::stringstream oss;
                        oss << "HTTP/1.1 404 Not Found\r\n\r\n";
                        conn_ptr->send(oss.str());
                        conn_ptr->close();
                        return;
                    }
                    bool keep_alive = true;
                    if(req_.has_header("Connection")) {
                        if(utils::iequal(req_.get_header_value("Connection"), "Close")) {
                            keep_alive = false;
                        }
                    }
                    log_debug(ip, port);
                    std::weak_ptr client_weak_conn{ conn_ptr };
                    auto read_cb = [client_weak_conn](auto proxy_conn_ptr) {
                        if(auto strong_conn = client_weak_conn.lock(); strong_conn) {
                            strong_conn->send(proxy_conn_ptr->recv_all());
                        }
                    };
                    auto close_cb = [keep_alive, client_weak_conn](auto) {
                        if(auto strong_conn = client_weak_conn.lock(); strong_conn) {
                            if(!keep_alive) {
                                log_info("close client connection");
                                strong_conn->close();
                            }
                        }
                    };
                    auto proxy_conn = net::TcpClient::connect(conn_ptr->loop(), ip, port, read_cb, nullptr, close_cb);
                    if(proxy_conn) {
                        conn_ptr->on_close([proxy_conn](auto) {
                            log_info("close proxy connection");
                            proxy_conn->close();
                        });
                        std::string recv_msg = conn_ptr->recv_all();
                        log_debug(recv_msg);
                        proxy_conn->send(recv_msg);
                    }
                    else {
                        conn_ptr->close();
                    }
                    parser_.clear();
                    parse_len_ = 0;
                }
            }
        private:
            Handler& handler_;
            HttpParser parser_;
            Request req_;
            Response res_;
            std::size_t parse_len_{ 0 };
    };
}
