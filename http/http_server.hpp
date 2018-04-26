#pragma once
#include "../cortono.hpp"

#include "http_router.hpp"
#include "http_connection.hpp"

namespace cortono::http
{
    template <typename Handler>
    class HttpServer
    {
        public:
            HttpServer(Handler& handler, const std::string& ip, unsigned short port, std::size_t concurrency)
                : service_(&loop_, ip, port),
                  handler_(handler),
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
                    auto c = std::make_shared<Connection<Handler>>(handler_);
                    conn_ptr->on_read([c](auto conn_ptr) {
                        c->handle_read(conn_ptr);
                    });
                });
            }
        private:
            net::EventLoop loop_;
            net::TcpService service_;

            Handler& handler_;
            std::size_t concurrency_;
    };
}
