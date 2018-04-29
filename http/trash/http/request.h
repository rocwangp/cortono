/*
 * =====================================================================================
 *
 *       Filename:  request.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2018年03月02日 17时10分32秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */

#pragma once
#include <iostream>

#include <string>
#include <sstream>
#include <memory>
#include <regex>
#include <map>

#include "../cortono.h"
#include "utils.h"

namespace cortono::http
{
    class request
    {
        public:
            enum parse_status {
                complete = 0,
                has_error = -1,
                not_complete = -2
            };
        public:
            parse_status parse_header(std::string info) {
                log_trace;
                std::stringstream info_stream(std::move(info));
                std::string line;
                std::getline(info_stream, line);
                line.pop_back();
                log_debug(line);
                std::match_results<std::string::const_iterator> match;
                std::regex e("^([^ ]+) ([^ ]+) HTTP/([^ ]+)$");
                log_debug(line);
                if(std::regex_match(line, match, e)) {
                    log_debug(line);
                    method_ = std::move(match[1]);
                    url_ = std::move(match[2]);
                    version_ = std::move(match[3]);
                    url_len_ = url_.length();

                    log_debug(method_, url_, version_);

                    std::string_view url { url_.data(), url_.length() };
                    std::cout << url << std::endl;
                    if(url.find('/') == std::string_view::npos) {
                        return parse_status::has_error;
                    }
                    if(auto pos = url.find('?'); pos != std::string_view::npos) {
                        parse_queries(url.substr(pos + 1));
                        url_len_ = pos;
                    }

                    bool matched = false;
                    e = "^([^:]+) ?: ?([.*])$";
                    do {
                        std::getline(info_stream, line);
                        line.pop_back();
                        if(matched = std::regex_match(line, match, e); matched)
                            headers_.emplace(std::move(match[1]), std::move(match[2]));
                    }while(matched);
                    log_trace;
                    return parse_status::complete;
                }
                log_trace;
                return parse_status::has_error;
            }

            void parse_queries(std::string_view str) {
                std::size_t pos = 0;
                std::string_view key, value;
                for(std::size_t i = 0; i != str.length(); ++i) {
                    if(str[i] == '=') {
                        key = { &str[i], i - pos };
                        pos = i + 1;
                    }
                    else if(str[i] == '&') {
                        value = { &str[pos], i - pos};
                        queries_.emplace(key, value);
                        pos = i + 1;
                    }
                }
            }

            bool is_complete() {
                return body_.length() == std::strtoul(headers_["content-length"].data(), nullptr, 10);
            }

            bool has_body() {
                if(auto it = headers_.find("content_length");
                   it == headers_.end() || it->second == "0") {
                    return false;
                }
                return true;
            }

            bool has_gzip() {
                if(auto it = headers_.find("content-encoding"); it != headers_.end()) {
                    if(it->second.find("gzip") != std::string::npos) {
                        return true;
                    }
                }
                return false;
            }

            bool uncompress() {

            }

            void set_body(std::string&& body) {
                body_.append(std::move(body));
            }

            std::string_view get_method() const {
                return { method_.data(), method_.length() };
            }

            std::string_view get_url() const {
                if(url_len_ != 0) {
                    return { url_.data(), url_len_ };
                }
                return {};
            }

            std::string_view get_header_value(const std::string& header) const {
                if(auto it = headers_.find(header); it != headers_.end()) {
                    return { it->second.data(), it->second.length() };
                }
                return {};
            }

            std::string_view get_query_value(std::string_view key) const {
                if(auto it = queries_.find(key); it != queries_.end()) {
                    return it->second;
                }
                return {};
            }

            bool is_http11() const {
                return version_ == "1.1";
            }
        private:
            std::size_t url_len_ = 0;
            std::string method_, url_, version_;
            std::map<std::string, std::string, ci_less> headers_;
            std::map<std::string_view, std::string_view> queries_;
            std::string body_;
    };
}
