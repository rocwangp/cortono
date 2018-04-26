#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_parser.hpp"
#include "http_response.hpp"

namespace cortono::http
{
    template <typename Handler>
    class Connection
    {
        public:
            Connection(Handler& handler)
                : handler_(handler)
            {
            }
            void handle_read(net::TcpConnection::Pointer conn_ptr) {
                int len = parser_.feed(conn_ptr->recv_buffer()->data(), conn_ptr->recv_buffer()->size());
                conn_ptr->recv_buffer()->retrieve_read_bytes(len);
                if(parser_.done()) {
                    req_ = std::move(parser_.to_request());
                    bool add_keep_alive = false;
                    bool is_invalid_request = false;
                    if(parser_.check_version(1, 0)) {
                        if(req_.has_header("connection")) {
                            if(utils::iequal(req_.get_header_value("connection"), "Keep-Alive")) {
                                add_keep_alive = true;
                            }
                        }
                    }
                    else if(parser_.check_version(1, 1)) {
                        add_keep_alive = true;
                        if(req_.has_header("connection")) {
                            if(utils::iequal(req_.get_header_value("connection"), "Close")) {
                                add_keep_alive = false;
                            }
                        }
                        if(!req_.has_header("host")) {
                            is_invalid_request = true;
                            res_ = Response(400);
                        }
                    }
                    if(add_keep_alive) {
                        res_.set_header("Connection", "Keep-Alive");
                    }
                    else {
                        res_.set_header("Connection", "Close");
                    }
                    if(!is_invalid_request) {
                        handler_.handle(req_, res_);
                    }
                    conn_ptr->send(std::move(complete_request()));
                    if(!add_keep_alive) {
                        conn_ptr->close();
                    }
                    parser_.clear();
                }
            }
        private:
            std::string complete_request() {
                static const std::unordered_map<int, std::string> status_codes = {
                    {200, "HTTP/1.1 200 OK\r\n"},
                    {201, "HTTP/1.1 201 Created\r\n"},
                    {202, "HTTP/1.1 202 Accepted\r\n"},
                    {204, "HTTP/1.1 204 No Content\r\n"},

                    {300, "HTTP/1.1 300 Multiple Choices\r\n"},
                    {301, "HTTP/1.1 301 Moved Permanently\r\n"},
                    {302, "HTTP/1.1 302 Moved Temporarily\r\n"},
                    {304, "HTTP/1.1 304 Not Modified\r\n"},

                    {400, "HTTP/1.1 400 Bad Request\r\n"},
                    {401, "HTTP/1.1 401 Unauthorized\r\n"},
                    {403, "HTTP/1.1 403 Forbidden\r\n"},
                    {404, "HTTP/1.1 404 Not Found\r\n"},
                    {413, "HTTP/1.1 413 Payload Too Large\r\n"},
                    {422, "HTTP/1.1 422 Unprocessable Entity\r\n"},
                    {429, "HTTP/1.1 429 Too Many Requests\r\n"},

                    {500, "HTTP/1.1 500 Internal Server Error\r\n"},
                    {501, "HTTP/1.1 501 Not Implemented\r\n"},
                    {502, "HTTP/1.1 502 Bad Gateway\r\n"},
                    {503, "HTTP/1.1 503 Service Unavailable\r\n"},
                };
                static const std::string crlf = "\r\n";
                static const std::string seperator = ": ";

                std::stringstream buffer;
                buffer << status_codes.find(res_.code)->second;
                if(res_.code >= 400 && res_.body.empty()) {
                    res_.body = status_codes.find(res_.code)->second.substr(9);
                }
                for(auto&& [key, value] : res_.headers) {
                    buffer << std::move(key) << seperator << std::move(value) << crlf;
                }
                if(!res_.headers.count("connection-length")) {
                    buffer << "Content-Length" << seperator << res_.body.size() << crlf;
                }
                buffer << crlf;
                buffer << res_.body;
                log_debug(buffer.str());
                return buffer.str();
            }
        private:
            Handler& handler_;
            HttpParser parser_;
            Request req_;
            Response res_;
    };
}
