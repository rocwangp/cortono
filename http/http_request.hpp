#pragma once
#include "../std.hpp"
#include "ci_map.hpp"

namespace cortono::http
{
    enum class HttpMethod
    {
        GET = 0,
        POST,
        CONNECT,

        METHOD_NUMS
    };

    struct Request
    {
        HttpMethod method;
        std::string raw_url, url, body;
        std::pair<int, int> version;
        std::unordered_map<std::string, std::string> query_kv_pairs;
        ci_map header_kv_pairs;

        const std::string& get_header_value(std::string&& key) {
            return header_kv_pairs[key];
        }
        bool has_header(std::string&& key) const {
            return header_kv_pairs.count(key);
        }
        std::string method_to_string() const {
            if(method == HttpMethod::GET) {
                return "GET";
            }
            else if(method == HttpMethod::POST) {
                return "POST";
            }
            else if(method== HttpMethod::CONNECT) {
                return "CONNECT";
            }
            else {
                return "GET";
            }
        }
    };
}
