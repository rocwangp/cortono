#pragma once

#include "http_request.hpp"
#include "http_response.hpp"
#include "../cortono.hpp"
#include "util.hpp"

namespace cortono::http
{
    using namespace cortono::net;

    class http_module
    {
        public:
            virtual ~http_module() {}

            virtual module_handle_status handle(http_request& req, http_response& res,
                                                std::shared_ptr<cort_socket> socket) = 0;
    };
}
