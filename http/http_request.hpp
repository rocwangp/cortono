#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_util.hpp"


namespace cortono::http
{
    using namespace std::literals;


    class http_request
    {
        public:
            http_request()
                : status_(parse_status::parse_line),
                  http_method_(http_method::unknown)
            {
            }
            parse_status parse_line(std::shared_ptr<cortono::net::event_buffer> read_buffer) {
                log_trace;
                std::string_view buf = read_buffer->read_sv_util("\r\n");
                if(!buf.empty()) {
                    std::regex e("^([a-zA-Z]+) ([^ ]+) HTTP/(.*)\r\n$");
                    std::match_results<std::string_view::const_iterator> match;
                    if(std::regex_match(buf.begin(), buf.end(), match, e)) {
                        read_buffer->retrieve_read_bytes(buf.length());
                        method_ = std::move(match[1]);
                        uri_ = std::move(match[2]);
                        version_ = std::move(match[3]);
                        status_ = parse_status::parse_header;
                        log_trace;
                    }
                    else {
                        status_ = parse_status::parse_error;
                    }
                }
                return status_;
            }
            parse_status parse_header(std::shared_ptr<cortono::net::event_buffer> read_buffer) {
                std::string_view buf = read_buffer->read_sv_util("\r\n");
                if(buf == "\r\n"sv) {
                    read_buffer->retrieve_read_bytes(buf.length());
                    parse_common_fields();
                    if(content_length_ > 0) {
                        status_ = parse_status::parse_body;
                    }
                    else {
                        status_ = parse_status::parse_done;
                    }
                }
                else if(!buf.empty()){
                    std::regex e("^([^ ]+): (.*)\r\n$");
                    std::match_results<std::string_view::const_iterator> match;
                    if(std::regex_match(buf.begin(), buf.end(), match, e)) {
                        read_buffer->retrieve_read_bytes(buf.length());
                        headers_.emplace(std::move(match[1]), std::move(match[2]));
                    }
                    else {
                        status_ = parse_status::parse_error;
                    }
                }
                return status_;
            }
            parse_status parse_body(std::shared_ptr<cortono::net::event_buffer> read_buffer) {
                if(read_buffer->size() == content_length_) {
                    status_ = parse_status::parse_done;
                }
                return status_;
            }
            void parse_common_fields() {
                parse_method();
                parse_query();
                parse_content_length();
                parse_keep_alive();
            }
            void parse_method() {
                http_method_ = to_method(method_);
            }
            void parse_query() {
                std::string_view uri = uri_;
                std::size_t pos = uri.find_first_of('?');
                if(pos == std::string::npos) {
                    return;
                }
                std::size_t prev = pos + 1;
                std::string_view key, value;
                for(std::size_t i = pos + 1; i < uri.length(); ++i) {
                    if(uri[i] == '=') {
                        key = uri.substr(prev, i - prev);
                        prev = i + 1;
                    }
                    else if(uri[i] == '&' || i == uri.length() - 1) {
                        value = uri.substr(prev, i - prev);
                        queries_.emplace(std::string{ key.data(), key.length() },
                                         std::string{ value.data(), value.length() });
                    }
                }
                uri_ = uri.substr(0, pos);
            }
            void parse_content_length() {
                if(auto it = headers_.find("content-length"); it != headers_.end()) {
                    content_length_ = util::from_chars(it->second);
                }
                else {
                    content_length_ = 0;
                }
            }
            void parse_keep_alive() {
                if(auto it = headers_.find("connection"); it != headers_.end()) {
                    if(iequal(it->second, "close")) {
                        keep_alive_ = false;
                        return;
                    }
                }
                keep_alive_ = true;
            }
            parse_status get_parse_status() {
                return status_;
            }
            http_method method() const {
                return http_method_;
            }
            std::string_view uri() const {
                return { uri_.data(), uri_.length() };
            }
            void set_body(std::string_view body) {
                body_ = body;
            }
            void set_uri(std::string&& uri) {
                uri_ = std::move(uri);
            }
            void set_uri(std::string_view uri) {
                uri_ = { uri.data(), uri.length() };
            }
            bool keep_alive() const {
                return keep_alive_;
            }
        private:
            bool keep_alive_ = false;
            int content_length_ = 0;
            parse_status status_;
            http_method http_method_;
            std::string method_, uri_, version_;
            std::string_view body_;
            std::map<std::string, std::string, cortono::util::ci_less> headers_;
            std::map<std::string, std::string, cortono::util::ci_less> queries_;
    };
}
