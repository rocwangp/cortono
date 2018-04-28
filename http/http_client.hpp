#pragma once

#include "../std.hpp"
#include "../cortono.hpp"

namespace cortono::http
{
    class HttpClient
    {
        public:
            using self_t = HttpClient;

            self_t& url(std::string&& req_url) {
                url_ = std::move(req_url);
                return *this;
            }
            self_t& ip(std::string&& req_ip) {
                ip_ = std::move(req_ip);
                return *this;
            }
            self_t& port(unsigned short req_port) {
                port_ = req_port;
                return *this;
            }
            self_t& version(int head, int tail) {
                version_.first = head;
                version_.second = tail;
                return *this;
            }
            self_t& keep_alive(bool req_keep_alive) {
                keep_alive_ = req_keep_alive;
                return *this;
            }
            void connect() {
                auto client_conn = net::TcpClient::connect(&loop_, ip_, port_);
                client_conn->send(gen_request());
                client_conn->on_read([](auto conn_ptr) {
                    log_info(conn_ptr->recv_all());
                });
                loop_.loop();
            }
            std::string gen_request() {
                std::stringstream oss;
                oss  << "GET " << url_ << " HTTP/" << version_.first << "." << version_.second << "\r\n";
                if(keep_alive_) {
                    oss << "Connection: Keep-Alive\r\n";
                }
                else {
                    oss << "Connection: Close\r\n";
                }
                if(version_.first == 1 && version_.second == 1) {
                    oss << "Host: 127.0.0.1:10000\r\n";
                }
                oss << "\r\n";
                log_debug(oss.str());
                return oss.str();
            }
        private:
            net::EventLoop loop_;
            std::pair<int, int> version_;
            std::string url_;
            std::string ip_;
            unsigned short port_{ 80 };
            bool keep_alive_{ false };
    };
}
