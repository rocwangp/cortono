#pragma once
#include "../std.hpp"
#include "http_server.hpp"
#include "http_router.hpp"
#include "http_proxy_server.hpp"
#include "http_router.hpp"

namespace cortono::http
{
    template <typename... Middlewares>
    class CortHttp
    {
        public:
            using self_t = CortHttp;
#ifdef CORTONO_USE_SSL
            using https_server_t = HttpsServer<CortHttp>;
            using https_proxy_server_t = HttpsProxyServer<CortHttp>;
#endif
            using http_server_t = HttpServer<CortHttp>;
            using http_proxy_server_t = HttpProxyServer<CortHttp>;

            DynamicRule& register_rule(const std::string& rule) {
                return router_.new_dynamic_rule(rule);
            }
            self_t& port(unsigned short port) {
                port_ = port;
                return *this;
            }
            self_t& bindaddr(std::string&& bindaddr) {
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
            self_t& https() {
                is_https_ = true;
                return *this;
            }
            void run() {
                if(is_proxy_server_) {
#ifdef CORTONO_USE_SSL
                    if(is_https_) {
                        https_proxy_server_ = std::make_unique<https_proxy_server_t>(*this, bindaddr_, port_, concurrency_);
                        https_proxy_server_->run();
                    }
                    else
#endif
                    {
                        http_proxy_server_ = std::make_unique<http_proxy_server_t>(*this, bindaddr_, port_, concurrency_);
                        http_proxy_server_->run();
                    }
                }
                else {
                    router_.volidate();
#ifdef CORTONO_USE_SSL
                    if(is_https_) {
                        https_server_ = std::make_unique<https_server_t>(*this, bindaddr_, port_, concurrency_);
                        https_server_->run();
                    }
                    else
#endif
                    {
                        http_server_ = std::make_unique<http_server_t>(*this, bindaddr_, port_, concurrency_);
                        http_server_->run();
                    }
                }
            }
            void handle(const Request& req, Response& res) {
                router_.handle(req, res);
            }
        private:
            Router router_;
            bool is_proxy_server_{ false };
            bool is_https_{ false };
            unsigned short port_{ 9999 };
            std::string bindaddr_ { "0.0.0.0" };
            std::size_t concurrency_{ 1 };
            std::unique_ptr<http_server_t> http_server_;
            std::unique_ptr<http_proxy_server_t> http_proxy_server_;
#ifdef CORTONO_USE_SSL
            std::unique_ptr<https_server_t> https_server_;
            std::unique_ptr<https_proxy_server_t> https_proxy_server_;
#endif
    };

    using SimpleApp = CortHttp<>;
}
