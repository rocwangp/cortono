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


            // TODO: 改用状态机解析HTTP请求报文段
            int feed(const char* buffer, int len) {
                int feed_len = 0;
                int ret = 0;
                do {
                    switch(state_) {
                        case ParseState::PARSE_LINE:
                            ret = try_parse_line(buffer, len);
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
#ifdef CORTONO_USE_SSL
                    std::regex e("^([^ ]+) ([^ ]+) HTTP/([0-9]).([0-9])\r\n$");
#else
                    std::regex e("^([^ ]+) ([^ ]+) HTTP/([0-9]).([0-9])\r\n$");
#endif
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
                    if((end - buffer + 1) == 2 && buffer[0] == '\r' && buffer[1] == '\n') {
                        feed_len += 2;
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
                    parse_multipart_form_data();
                }
                return len;
            }
            std::string_view parse_multipart_form_data_boundary() const {
                auto it = header_kv_pairs_.find("content-type");
                if(it == header_kv_pairs_.end())
                    return {  };
                auto content_types = utils::split(it->second, "; ");
                if(content_types.size() <= 1)
                    return {  };
                if(!utils::iequal(content_types[0].data(), content_types[0].length(), "multipart/form-data"))
                    return {  };
                return utils::split(content_types[1], "=")[1];
            }
            void parse_multipart_form_data() {
                std::string_view boundary = parse_multipart_form_data_boundary();
                if(boundary.empty())
                    return;
                std::regex e("^([^:]+): (.*)\r\n$");
                std::match_results<std::string_view::const_iterator> match;
                std::size_t start_idx = body_.find(boundary.data(), 0, boundary.length());
                while(start_idx != std::string::npos) {
                    // skip boundary and \r\n
                    start_idx += boundary.length() + 2;
                    // find next boundary, the data between start_idx and end_idx is the file
                    std::size_t end_idx = body_.find(boundary.data(), start_idx, boundary.length());
                    if(end_idx == std::string::npos) 
                        break;

                    // --${boundary}
                    // Content-Disposition: form-data; name=...; filename=...\r\n
                    // Content-Type: ...\r\n
                    // \r\n
                    // ...file data\r\n
                    // --${boundary}

                    UploadFile upload_file;
                    while(true) {
                        std::size_t pos = body_.find("\r\n", start_idx, 2);
                        if(start_idx == pos) {
                            start_idx += 2;
                            break;
                        }
                        std::string_view line(&body_[start_idx], pos + 2 - start_idx);
                        if(std::regex_match(line.cbegin(), line.cend(), match, e)) {
                            if(utils::iequal(match[1].str().data(), match[1].length(), "Content-Disposition")) {
                                std::string value = match[2];
                                std::vector<std::string_view> split_value = utils::split(value, "; ");
                                std::string_view filename = utils::split(split_value.back(), "=").back();
                                if(filename.front() == '\"') {
                                    upload_file.filename.assign(filename.data() + 1, filename.length() - 2);
                                }
                                else {
                                    upload_file.filename.assign(filename.data(), filename.length());
                                }
                            }
                            else if(utils::iequal(match[1].str().data(), match[1].length(), "Content-Type")) {
                                upload_file.filetype = match[2];
                            }
                        }
                        else {
                            break;
                        }

                        start_idx = pos + 2;
                    }
                    // end_idx - 4指向file data后面的\r
                    upload_file.content = body_.substr(start_idx, end_idx - 4 - start_idx);
                    upload_files_.emplace_back(std::move(upload_file));

                    start_idx = end_idx;
                }
                // if(header_kv_pairs_.count("content-type")) {
                    // auto content_types = utils::split(header_kv_pairs_["content-type"], "; ");
                    // if(content_types.size() > 1 && utils::iequal(content_types.front().data(), content_types.front().size(), "multipart/form-data")) {
                        // auto boundary = utils::split(content_types[1], "=")[1];
                        // auto start_idx = body_.find(boundary.data(), 0, boundary.length());
                        // log_info(start_idx);
                        // if(start_idx == std::string::npos)
                            // return;
                        // start_idx += boundary.length() + 2;
                        // auto end_idx = body_.find(boundary.data(), start_idx, boundary.length());
                        // if(end_idx == std::string::npos)
                            // return;
                        // end_idx -= 4;
//
                        // std::regex e("^([^:]+): (.*)\r\n$");
                        // std::match_results<std::string::const_iterator> match;
                        // while(true) {
                            // auto pos = body_.find_first_of("\r\n", start_idx);
                            // if(start_idx == pos) {
                                // start_idx += 2;
                                // break;
                            // }
                            // std::string line(body_.data() + start_idx, body_.data() + pos + 2);
                            // if(std::regex_match(line.cbegin(), line.cend(), match, e)) {
                                // upload_kv_pairs_[std::move(match[1])] = std::move(match[2]);
                                // start_idx = pos + 2;
                            // }
                        // }
                        // body_ = body_.substr(start_idx, end_idx - start_idx);
                    // }
                // }
            }
            void parse_method(std::string&& method) {
                log_info(method);
                if(utils::iequal(method.data(), method.length(), "GET")) {
                    method_ = HttpMethod::GET;
                }
                else if(utils::iequal(method.data(), method.length(), "POST")) {
                    method_ = HttpMethod::POST;
                }
                else if(utils::iequal(method.data(), method.length(), "CONNECT")) {
                    method_ = HttpMethod::CONNECT;
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
                return Request { method_, raw_url_, req_url_, body_, version_, query_kv_pairs_, header_kv_pairs_, upload_files_ };
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
            // ci_map upload_kv_pairs_;
            std::vector<UploadFile> upload_files_;
    };
};
