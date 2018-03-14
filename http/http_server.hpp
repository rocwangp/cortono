#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_session.hpp"

namespace cortono::http
{
    using namespace cortono::net;
    using namespace std::literals;

    class HttpServer
    {
        public:
            HttpServer(std::string_view ip, unsigned short port)
                : service_(&base_, ip, port)
            {
                init_callback();
            }
            void start() {
                config_load();
                service_.start();
                base_.loop();
            }
        private:
            void config_load() {

            }
            void init_callback() {
                service_.on_conn([this](auto conn) {
                    log_trace;
                    {
                        /* std::unique_lock lock { mutex_ }; */
                        auto session = std::make_shared<HttpSession>();
                        sessions_[conn] = session;
                        conn->on_read([=](auto conn) { session->parse_line(conn); });
                    }
                });
                service_.on_close([this](auto conn) {
                    {
                        std::unique_lock lock { mutex_ };
                        std::weak_ptr weak_conn { conn };
                        sessions_.erase(weak_conn);
                    }
                });
            }
        private:
            EventLoop base_;
            TcpService service_;
            std::mutex mutex_;
            std::map<std::weak_ptr<TcpConnection>,
                     std::shared_ptr<HttpSession>,
                     std::owner_less<std::weak_ptr<TcpConnection>>> sessions_;
    };
}
