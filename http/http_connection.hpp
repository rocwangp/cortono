#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_parser.hpp"
#include "http_response.hpp"

namespace cortono::http
{
    // Connection仅仅用来定义不同的连接类型，作为handle_read的参数
    template <typename Handler, typename Connection>
    class WebConnection
    {
        public:
            WebConnection(Handler& handler)
                : handler_(handler)
            {
            }
            // 对于TCP和SSL，handle_read的处理完全相同
            // 不同之处完全隐藏在TcpConnection和SslConnection的同名接口下
            void handle_read(typename Connection::Pointer& conn_ptr) {
                /* log_debug(conn_ptr->recv_buffer()->to_string()); */
                int len = parser_.feed(conn_ptr->recv_buffer()->data(), conn_ptr->recv_buffer()->size());
                conn_ptr->recv_buffer()->retrieve_read_bytes(len);
                if(parser_.done()) {
                    log_trace;
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
                            log_info("no host, return Response(400)");
                        }
                    }
                    if(!is_invalid_request) {
                        log_info("handle request");
                        handler_.handle(req_, res_);
                    }
                    if(add_keep_alive) {
                        res_.set_header("Connection", "Keep-Alive");
                    }
                    else {
                        res_.set_header("Connection", "Close");
                    }
                    log_trace;
                    auto [sendfile, context] = std::move(complete_request());
                    conn_ptr->send(context);
                    if(sendfile) {
                        log_info("start send file");
                        conn_ptr->sendfile(res_.filename);
                    }
                    if(!add_keep_alive) {
                        log_info("no keep-alive, close connection");
                        conn_ptr->close();
                    }
                    parser_.clear();
                }
            }
        private:
            std::pair<bool, std::string> complete_request() {
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
                    if(!res_.sendfile) {
                        buffer << "Content-Length" << seperator << res_.body.size() << crlf;
                    }
                    else {
                        buffer << "Content-Length" << seperator << res_.filesize << crlf;
                    }
                }
                buffer << crlf;
                log_debug(buffer.str());
                if(res_.is_send_file()) {
                    return { true, buffer.str() };
                }
                else {
                    buffer << res_.body;
                    return { false, buffer.str() };
                }
            }
        private:
            Handler& handler_;
            HttpParser parser_;
            Request req_;
            Response res_;
    };
}
