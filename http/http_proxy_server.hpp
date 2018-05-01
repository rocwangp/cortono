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
                    auto proxy_conn = std::make_shared<WebProxyConnection<Handler, Client, ServiceConnType>>(handler_);
                    conn_ptr->on_read([proxy_conn = std::move(proxy_conn)](auto conn_ptr) {
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
    // 这里做了简化，对于HTTPS代理，处理浏览器->代理服务器的证书问题比较麻烦，所以直接改为HTTP处理
    // 使用方法：将浏览器的HTTPS代理设置成默认走HTTP连接即可
    // 这样无需处理浏览器->代理服务器的证书问题
    // 由于数据走TCP连接，所以代理服务器可以获取请求头
    // 因为浏览器发送的仍然是HTTPS数据，所以请求体（数据）仍是加密的，所以仍然可以保证安全性
    // 当获取到CONNECT中的<ip, port>后，仍然采用TCP连接目标服务器，返回HTTP/1,1 200 Connection Established\r\n\r\n给浏览器
    // 此后只需要转发即可，由于客户端发送的数据仍然是加密的，所以只需要无条件转发给目标服务器即可
    // （虽然采用TCP而非SSL连接目标服务器，但是目标服务器实际上仍然运行在HTTPS上，可以处理加密后的数据）
    template <typename Handler>
    using HttpsProxyServer = WebProxyServer<Handler, net::TcpClient, net::EventLoop, net::TcpService>;
#endif
}
