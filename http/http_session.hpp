#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_util.hpp"
#include "http_mime_type.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "response_cv.hpp"

#include <experimental/filesystem>

namespace cortono::http
{
    using namespace cortono::net;
    using namespace std::experimental;

    class HttpSession
    {
        public:
            void parse_line(TcpConnection::Pointer conn) {
                log_trace;
                auto status = request_.parse_line(conn->recv_buffer());
                switch(status) {
                    case ParseStatus::ParseError:
                        response_back(conn, ResponseStatus::BadRequest);
                        break;
                    case ParseStatus::ParseHeader:
                        conn->on_read(std::bind(
                            &HttpSession::parse_header, this, std::placeholders::_1));
                        parse_header(conn);
                        break;
                    default:
                        ;
                }
            }
            void parse_header(TcpConnection::Pointer conn) {
                log_trace;
                auto recv_buffer = conn->recv_buffer();
                while(true) {
                    auto status = request_.parse_header(recv_buffer);
                    if(status == ParseStatus::ParseBody) {
                        log_trace;
                        conn->on_read(std::bind(
                            &HttpSession::parse_body, this, std::placeholders::_1));
                        parse_body(conn);
                        break;
                    }
                    else if(status == ParseStatus::ParseError) {
                        break;
                    }
                    else if(status == ParseStatus::NoComplete) {
                        break;
                    }
                }
            }
            void parse_body(TcpConnection::Pointer conn) {
                log_trace;
                auto status = request_.parse_body(conn->recv_buffer());
                if(status == ParseStatus::ParseDone) {
                    conn->on_read([this](auto conn) { handle_none(conn); });
                    handle_request(conn);
                }
            }
            void handle_none(TcpConnection::Pointer) {

            }
            void handle_request(TcpConnection::Pointer conn) {
                log_trace;
                request_.set_body(conn->recv_string_view());
                if(request_.static_file()) {
                    auto file_path = request_.file_path();

                    log_debug(std::string{file_path.data(), file_path.length()});
                    filesystem::path p{ file_path };
                    if(!filesystem::exists(p)) {
                        log_trace;
                        response_back(conn, ResponseStatus::BadRequest);
                        return;
                    }
                    std::size_t dot_index = file_path.find_last_of('.');
                    if(dot_index == std::string_view::npos) {
                        log_trace;
                        response_back(conn, ResponseStatus::BadRequest);
                        return;
                    }
                    auto content_type = get_mime_type(file_path.substr(dot_index));
                    auto file_size = filesystem::file_size(p);
                    response_.set_header(ResponseHeader::Content_Type, content_type);
                    response_.set_header(ResponseHeader::Content_Length, file_size);
                    if(request_.keep_alive()) {
                        response_.set_header(ResponseHeader::Connection, "keep-alive");
                    }
                    else {
                        response_.set_header(ResponseHeader::Connection, "close");
                    }
                    response_.set_status_and_content(ResponseStatus::OK);
                    auto header = response_.get_response_header();
                    log_debug(header);
                    conn->send(header);
                    conn->sendfile(file_path);

                    /* std::ifstream fin; */
                    /* fin.open(std::string{file_path.data(), file_path.length()}, std::ios_base::in); */
                    /* std::string content{ std::istream_iterator<char>{fin}, std::istream_iterator<char>{} }; */
                    /* fin.close(); */
                    /* conn->send(content); */

                    if(!request_.keep_alive())
                        conn->close();
                    conn->on_read([this](auto conn) { parse_line(conn); });
                    request_.reset();
                    response_.reset();
                }
                else {
                    response_back(conn, ResponseStatus::BadRequest);
                }
            }
            void response_back(TcpConnection::Pointer conn, ResponseStatus status) {
                response_.set_status_and_content(status);
                conn->send(response_.to_string());
                conn->close();
            }

        private:
            HttpRequest request_;
            HttpResponse response_;
    };

}
