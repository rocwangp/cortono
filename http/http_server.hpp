#pragma once
#include "../cortono.hpp"

#include "http_router.hpp"
#include "http_connection.hpp"

namespace cortono::http
{
    template <typename Handler, typename EventLoop, typename Service>
    class WebService
    {
        public:
            using ServiceConnType = typename black_magic::template_arg_traits<Service>::template arg_type<0>;

            WebService(Handler& handler, const std::string& ip, unsigned short port, std::size_t concurrency)
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
                    log_info(conn_ptr->name());
                    auto c = std::make_shared<WebConnection<Handler, ServiceConnType>>(handler_);
                    conn_ptr->on_read([c = std::move(c)](auto conn_ptr) {
                        c->handle_read(conn_ptr);
                    });
                });
            }
        private:
            EventLoop loop_;
            Service service_;
            Handler& handler_;
            std::size_t concurrency_;
    };

    template <typename Handler>
    using HttpServer = WebService<Handler, net::EventLoop, net::TcpService>;

#ifdef CORTONO_USE_SSL
    template <typename Handler>
    using HttpsServer = WebService<Handler, net::EventLoop, net::SslService>;
#endif
}
