#pragma once

#include "../cortono.hpp"
#include "../std.hpp"
#include "http_parser.hpp"
#include "http_response.hpp"
#include "http_utils.hpp"
#include "http_proxy_connection.hpp"

namespace cortono::http
{
    template <typename Handler, typename Client, typename EventLoop, typename Service>
    class WebProxyServer
    {
        public:
            using ServiceConnType = typename black_magic::template_arg_traits<Service>::template arg_type<0>;

            WebProxyServer(Handler& handler, const std::string& ip, unsigned short port, std::size_t concurrency)
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
                    log_trace;
                    auto proxy_conn = std::make_shared<WebProxyConnection<Handler, Client, ServiceConnType>>(handler_);
                    conn_ptr->on_read([proxy_conn = std::move(proxy_conn)](auto conn_ptr) {
                        log_trace;
                        proxy_conn->handle_read(conn_ptr);
                    });
                });
            }
        private:
            Handler& handler_;
            EventLoop loop_;
            Service service_;
            std::size_t concurrency_{ 1 };
    };

    template <typename Handler>
    using HttpProxyServer = WebProxyServer<Handler, net::TcpClient, net::EventLoop, net::TcpService>;

#ifdef CORTONO_USE_SSL
    template <typename Handler>
    using HttpsProxyServer = WebProxyServer<Handler, net::TcpClient, net::EventLoop, net::TcpService>;
#endif
}
