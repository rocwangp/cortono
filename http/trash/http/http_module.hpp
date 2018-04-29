#pragma once

#include "../cortono.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "http_util.hpp"

namespace cortono::http
{
    using namespace cortono::net;

    class http_module
    {
        public:
            virtual ~http_module() {}

            virtual module_handle_status handle(http_request& req, http_response& res,
                                                std::shared_ptr<tcp_socket> socket) = 0;
    };
}
