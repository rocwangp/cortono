#pragma once

#include <string>
#include <string_view>

namespace cortono::http
{
    enum class status_type
    {
        init,
        ok = 200,
        bad_request = 400,
        not_found = 404
    };


    using namespace std::literals;

    constexpr inline auto rep_ok = "HTTP/1.1 200 OK\r\n"sv;
    constexpr inline auto rep_bad_request = "HTTP/1.1 400 Bad Request\r\n"sv;
    constexpr inline auto rep_not_found = "HTTP/1.1 404 Not Found\r\n"sv;

    constexpr inline auto name_value_separator = ": "sv;
    constexpr inline auto crlf = "\r\n"sv;

    inline std::string_view status_to_sv(status_type status) {
        switch(status) {
            case status_type::ok:
                return rep_ok;
            case status_type::bad_request:
                return rep_bad_request;
            case status_type::not_found:
                return rep_not_found;
            default:
                return {};
        }
    }


}
