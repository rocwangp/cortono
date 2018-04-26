#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "ci_map.hpp"
#include "http_request.hpp"
#include "http_utils.hpp"

namespace cortono::http
{
    class HttpParser
    {
        public:
            enum class ParseState
            {
                PARSE_LINE,
                PARSE_HEADER,
                PARSE_BODY,
                PARSE_DONE,
                PARSE_ERROR
            };
            int feed(const char* buffer, int len) {
                int feed_len = 0;
                int ret = 0;
                do {
                    switch(state_) {
                        case ParseState::PARSE_LINE:
                            ret = try_parse_line(buffer + feed_len, len);
                            break;
                        case ParseState::PARSE_HEADER:
                            ret = try_parse_header(buffer, len);
                            break;
                        case ParseState::PARSE_BODY:
                            ret = try_parse_body(buffer, len);
                            break;
                        default:
                            ret = 0;
                            break;
                    }
                    buffer += ret;
                    len -= ret;
                    feed_len += ret;
                }while(ret != 0 && state_ != ParseState::PARSE_ERROR);
                return feed_len;
            }
            int try_parse_line(const char* buffer, int len) {
                const char* end = std::find(buffer, buffer + len, '\n');
                if(end != buffer + len) {
                    std::string_view sv(buffer, end - buffer + 1);
                    std::regex e("^([^ ]+) ([^ ]+) HTTP/([0-9]).([0-9])\r\n$");
                    std::match_results<std::string_view::const_iterator> match;
                    if(std::regex_match(sv.cbegin(), sv.cend(), match, e)) {
                        parse_method(std::move(match[1]));
                        parse_url(std::move(match[2]));
                        parse_version(std::move(match[3]), std::move(match[4]));
                        state_ = ParseState::PARSE_HEADER;
                        return end - buffer + 1;
                    }
                }
                return 0;
            }
            int try_parse_header(const char* buffer, int len) {
                std::regex e("^([^:]+): (.*)\r\n$");
                std::match_results<std::string_view::const_iterator> match;
                const char* end{ nullptr };
                int feed_len{ 0 };
                while((end = std::find(buffer, buffer + len, '\n')) != buffer + len) {
                    if(len == 2 && buffer[0] == '\r' && buffer[1] == '\n') {
                        feed_len += len;
                        state_ = ParseState::PARSE_BODY;
                        break;
                    }
                    int n = end - buffer + 1;
                    std::string_view sv(buffer, n);
                    if(std::regex_match(sv.cbegin(), sv.cend(), match, e)) {
                        header_kv_pairs_.emplace(std::move(match[1]), std::move(match[2]));
                        len -= n;
                        buffer = end + 1;
                        feed_len += n;
                    }
                    else {
                        state_ = ParseState::PARSE_ERROR;
                        return 0;
                    }
                }
                return feed_len;
            }
            int try_parse_body(const char* buffer, int len) {
                body_.append(buffer, len);
                if(!header_kv_pairs_.count("content-length") ||
                   (body_.length() == std::strtoul(header_kv_pairs_["content-length"].data(), nullptr, 10))) {
                    state_ = ParseState::PARSE_DONE;
                }
                return len;
            }
            void parse_method(std::string&& method) {
                if(utils::iequal(method.data(), method.length(), "GET")) {
                    method_ = HttpMethod::GET;
                }
                else if(utils::iequal(method.data(), method.length(), "POST")) {
                    method_ = HttpMethod::POST;
                }
                else {
                    method_ = HttpMethod::GET;
                }
            }
            void parse_url(std::string&& url) {
                raw_url_ = url;
                if(auto it = std::find(url.begin(), url.end(), '?');
                    it != url.end()) {
                    std::string query_url(it + 1, url.end());
                    url.resize(it - url.begin());
                    parse_query_kv_pairs(std::move(query_url));
                }
                req_url_ = std::move(url);
            }
            void parse_query_kv_pairs(std::string&& query_url) {
                std::size_t front = 0, back = 0;
                std::string key, value;
                while(back <= query_url.length()) {
                    if(back == query_url.length() || query_url[back] == '&') {
                        value.assign(query_url.begin() + front, query_url.begin() + back);
                        front = back + 1;
                        query_kv_pairs_.emplace(std::move(key), std::move(value));
                    }
                    else if(query_url[back] == '=') {
                        key.assign(query_url.begin() + front, query_url.begin() + back);
                        front = back + 1;
                    }
                    ++back;
                }
            }
            void parse_version(std::string&& head, std::string&& tail) {
                version_.first = std::atoi(head.data());
                version_.second = std::atoi(tail.data());
            }
            Request to_request() const {
                return Request { method_, raw_url_, req_url_, body_, version_, query_kv_pairs_, header_kv_pairs_ };
            }
            bool done() const {
                return state_ == ParseState::PARSE_DONE;
            }
            bool check_version(int head, int tail) const {
                return version_.first == head && version_.second == tail;
            }
            void clear() {
                state_ = ParseState::PARSE_LINE;
                method_ = HttpMethod::GET;
                raw_url_.clear();
                req_url_.clear();
                body_.clear();
                query_kv_pairs_.clear();
                header_kv_pairs_.clear();
            }
        private:
            ParseState state_ { ParseState::PARSE_LINE };
            HttpMethod method_;
            std::string raw_url_, req_url_, body_;
            std::pair<int, int> version_;
            std::unordered_map<std::string, std::string> query_kv_pairs_;
            ci_map header_kv_pairs_;
    };
};
