#pragma once

#include "../cortono.hpp"

#include "http_module.hpp"
#include "util.hpp"

namespace cortono::http
{
    using namespace cortono::net;

    class http_index_module : public http_module
    {
        public:
            http_index_module() : http_module() {}
            virtual ~http_index_module() {}
            virtual module_handle_status handle(http_request& req, http_response& res,
                                                std::shared_ptr<cort_socket> socket) override {
                log_trace;
                if(auto method = req.method(); method != GET) {
                    return module_handle_status::done;
                }

                std::string_view uri = req.uri();
                if(uri == "/"sv) {
                    req.rewrite_uri("web/index.html"sv);
                    return module_handle_status::interval_redirect;
                }
                else {
                    return module_handle_status::done;
                }
            }
    };
}

