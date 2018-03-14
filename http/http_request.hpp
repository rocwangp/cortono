#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_codec.hpp"
#include "http_util.hpp"


namespace cortono::http
{
    using namespace std::literals;


    class HttpRequest
    {
        public:
            HttpRequest()
                : status_(ParseStatus::ParseLine),
                  HttpMethod_(HttpMethod::UNKNOWN)
            {
            }
            ParseStatus parse_line(std::shared_ptr<cortono::net::Buffer> read_buffer) {
                std::string_view buf = read_buffer->read_sv_util("\r\n");
                if(!buf.empty()) {
                    std::regex e("^([a-zA-Z]+) ([^ ]+) HTTP/(.*)\r\n$");
                    std::match_results<std::string_view::const_iterator> match;
                    if(std::regex_match(buf.begin(), buf.end(), match, e)) {
                        read_buffer->retrieve_read_bytes(buf.length());
                        method_ = std::move(match[1]);
                        uri_ = std::move(match[2]);
                        version_ = std::move(match[3]);
                        handle_request_line();
                        handle_request_uri();
                        status_ = ParseStatus::ParseHeader;
                    }
                    else {
                        status_ = ParseStatus::ParseError;
                    }
                }
                return status_;
            }

            void handle_request_line() {
                if(version_ != "1.1") {
                    status_ = ParseStatus::ParseError;
                }
            }
            void parse_request_query() {
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
            void handle_request_uri() {
                parse_request_query();
                if(uri_.back() == '/')
                    uri_.append("index.html");
                uri_ = std::string("web") + uri_;
                static_file_ = true;
                uri_ = html_codec::decode(uri_);
                log_debug(uri_);
            }

            ParseStatus parse_header(std::shared_ptr<cortono::net::Buffer> read_buffer) {
                std::string_view buf = read_buffer->read_sv_util("\r\n");
                if(buf == "\r\n"sv) {
                    read_buffer->retrieve_read_bytes(buf.length());
                    parse_common_fields();
                    status_ = ParseStatus::ParseBody;
                }
                else if(!buf.empty()){
                    std::regex e("^([^ ]+): (.*)\r\n$");
                    std::match_results<std::string_view::const_iterator> match;
                    if(std::regex_match(buf.begin(), buf.end(), match, e)) {
                        read_buffer->retrieve_read_bytes(buf.length());
                        headers_.emplace(std::move(match[1]), std::move(match[2]));
                    }
                    else {
                        status_ = ParseStatus::ParseError;
                    }
                }
                else {
                    status_ = ParseStatus::NoComplete;
                }
                return status_;
            }
            ParseStatus parse_body(std::shared_ptr<cortono::net::Buffer> read_buffer) {
                if(read_buffer->size() == content_length_) {
                    status_ = ParseStatus::ParseDone;
                }
                return status_;
            }
            void parse_common_fields() {
                parse_method();
                parse_content_length();
                parse_keep_alive();
            }
            void parse_method() {
                HttpMethod_ = to_method(method_);
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
            ParseStatus get_ParseStatus() {
                return status_;
            }
            HttpMethod method() const {
                return HttpMethod_;
            }
            std::string_view uri() const {
                return { uri_.data(), uri_.length() };
            }
            std::string_view file_path() const {
                return uri();
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
            bool static_file() const {
                return static_file_;
            }
            void reset() {
                keep_alive_ = true;
                static_file_ = true;
                content_length_ = 0;
                status_ = ParseStatus::ParseLine;
                method_.clear();
                uri_.clear();
                version_.clear();
                headers_.clear();
                queries_.clear();
            }
        private:
            bool keep_alive_ = false;
            bool static_file_ = true;
            int content_length_ = 0;
            ParseStatus status_;
            HttpMethod HttpMethod_;
            std::string method_, uri_, version_;
            std::string_view body_;
            std::map<std::string, std::string, cortono::util::ci_less> headers_;
            std::map<std::string, std::string, cortono::util::ci_less> queries_;
    };
}
