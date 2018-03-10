#pragma once

#include "../cortono.hpp"

#include <string>
#include <string_view>

namespace cortono::http
{
    using namespace cortono::util;

    enum class http_method
    {
        get,
        post,
        head,
        options,
        put,
        trace,
        unknown,

        GET,
        POST,
        HEAD,
        OPTIONS,
        PUT,
        TRACE,
        UNKNOWN
    };

    constexpr inline auto GET = http_method::get;
    constexpr inline auto POST = http_method::post;

    enum class parse_status
    {
        parse_line,
        parse_header,
        parse_body,
        parse_done,
        parse_error
    };


    inline http_method to_method(std::string_view method) {
        log_debug(std::string{ method.data(), method.length() });
        if(auto s = to_lower(method); s == "get") {
            return GET;
        }
        else if(s == "post") {
            return POST;
        }
        return http_method::UNKNOWN;
    }


    enum class module_handle_status
    {
        interval_redirect,
        done,
        error
    };
}
