#pragma once

#include "../std.hpp"
#include "http_server.hpp"
#include "http_router.hpp"
#include "http_proxy_server.hpp"

namespace cortono::http
{
    template <typename... Middlewares>
    class CortHttp
    {
        public:
            using self_t = CortHttp;
            using server_t = HttpServer<CortHttp>;
            using proxy_server_t = HttpProxyServer<CortHttp>;

            DynamicRule& register_rule(std::string&& rule) {
                return router_.new_dynamic_rule(std::forward<std::string>(rule));
            }
            self_t& port(unsigned short port) {
                port_ = port;
                return *this;
            }
            self_t& bindaddr(std::string bindaddr) {
                bindaddr_ = std::move(bindaddr);
                return *this;
            }
            self_t& concurrency(std::size_t c) {
                concurrency_ = c;
                return *this;
            }
            self_t& multithread() {
                concurrency_ = std::thread::hardware_concurrency();
                return *this;
            }
            self_t& proxy_server() {
                is_proxy_server_ = true;
                return *this;
            }
            void run() {
                if(is_proxy_server_) {
                    proxy_server_ = std::make_unique<proxy_server_t>(*this, bindaddr_, port_, concurrency_);
                    proxy_server_->run();
                }
                else {
                    router_.volidate();
                    server_ = std::make_unique<server_t>(*this, bindaddr_, port_, concurrency_);
                    server_->run();
                }
            }
            void handle(const Request& req, Response& res) {
                router_.handle(req, res);
            }
        private:
            Router router_;
            bool is_proxy_server_{ false };
            unsigned short port_{ 9999 };
            std::string bindaddr_ { "0.0.0.0" };
            std::size_t concurrency_{ 1 };
            std::unique_ptr<server_t> server_;
            std::unique_ptr<proxy_server_t> proxy_server_;
    };

    using SimpleApp = CortHttp<>;
}
