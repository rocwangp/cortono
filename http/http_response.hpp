#pragma once

#include "../std.hpp"
namespace cortono::http
{
    struct Response
    {
        int code{ 200 };
        std::string body;
        std::unordered_map<std::string, std::string> headers;

        Response() {}
        explicit Response(int state_code) : code(state_code) {}
        explicit Response(std::string&& context) : body(std::move(context)) {}
        Response(int state_code, std::string&& context) : code(state_code), body(std::move(context)) {}

        Response(Response&& res)
            : code(res.code),
              body(std::move(res.body)),
              headers(std::move(res.headers))
        {  }

        Response& operator=(Response&& res) {
            if(this != &res) {
                code = std::move(res.code);
                body = std::move(res.body);
                headers = std::move(res.headers);
            }
            return *this;
        }

        void set_header(std::string&& key, std::string&& value) {
            headers.emplace(std::forward<std::string>(key),
                                    std::forward<std::string>(value));
        }
        bool has_header(std::string&& key) const {
            return headers.count(key);
        }
        const std::string& get_header_value(std::string&& key)  {
            return headers[key];
        }
    };
}
