#pragma once

#include "../std.hpp"
#include "../cortono.hpp"

namespace cortono::http
{
    using namespace cortono::util;

    enum class HttpMethod
    {
        GET,
        POST,
        HEAD,
        OPTIONS,
        PUT,
        TRACE,
        UNKNOWN
    };

    constexpr inline auto GET = HttpMethod::GET;
    constexpr inline auto POST = HttpMethod::POST;

    enum class ParseStatus
    {
        ParseLine,
        ParseHeader,
        ParseBody,
        ParseError,
        ParseDone,
        NoComplete
    };


    inline HttpMethod to_method(std::string_view method) {
        if(auto s = to_lower(method); s == "get") {
            return GET;
        }
        else if(s == "post") {
            return POST;
        }
        return HttpMethod::UNKNOWN;
    }


    enum class module_handle_status
    {
        done,
        next,
        error,
        redirection
    };
}
