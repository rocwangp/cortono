#pragma once
#include "../std.hpp"
#include "ci_map.hpp"
#include "http_session_manager.hpp"

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
        ci_map upload_kv_pairs;

        const std::string& get_header_value(std::string&& key) {
            return header_kv_pairs[key];
        }
        auto get_cookie_map(const std::string& cookie_str) const {
            std::unordered_map<std::string_view, std::string_view> cookie_map;
            auto cookie_vec = utils::split(cookie_str, "; ");
            for(auto cookie : cookie_vec) {
                auto v = utils::split(cookie, "=");
                if(v.size() == 2) {
                    cookie_map[v[0]] = v[1];
                }
            }
            return cookie_map;
        }
        std::weak_ptr<Session> get_session(const std::string& name) const {
            auto cookie_it = header_kv_pairs.find("cookie");
            if(cookie_it != header_kv_pairs.end()) {
                auto cookie_str = cookie_it->second;
                auto cookie_map = get_cookie_map(cookie_str);
                auto it = cookie_map.find(name);
                if(it != cookie_map.end()) {
                    return session_manager::get_session(std::string(it->second.data(), it->second.length()));
                }
            }
            return {};
        }
        std::weak_ptr<Session> get_session() const {
            static const std::string CORTONO_SESSIONID = "SESSIONID";
            return get_session(CORTONO_SESSIONID);
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
