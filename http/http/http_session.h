#pragma once
#include "../cortono.h"
#include "utils.h"
#include "request.h"
#include "response.h"
#include <memory>
#include <map>
#include <unordered_map>


namespace cortono::http
{
    using namespace cortono::net;
    using http_handler = std::function<void(const request&, response&)>;

    class http_session : public cort_session
    {
        public:
            http_session(std::shared_ptr<cort_socket> socket, http_handler handler)
                : cort_session(socket),
                  http_handler_(handler)
            {

            }

            ~http_session() {}

            void on_read() {
                log_trace;
                if(auto info = socket_->read_util("\r\n\r\n"); !info.empty()) {
                    auto status = req_.parse_header(info);
                    if(status == request::parse_status::has_error) {
                        log_error;
                        response_back(status_type::bad_request);
                        return;
                    }
                    log_trace;
                    check_keep_alive();
                    if(req_.has_body()){
                        req_.set_body(socket_->read_all());
                        content_type type = get_content_type();
                        switch(type) {
                            case content_type::string:
                            case content_type::unknown:
                                handle_string_body();
                                return;
                        }
                    }
                    else {
                        handle_header_request();
                    }
                }
                else {
                    log_error;
                    response_back(status_type::bad_request);
                }
            }

        private:

            content_type get_content_type() {
                return content_type::unknown;
            }

            void handle_string_body() {
                handle_body();
            }

            void handle_header_request() {
                call_back();
                do_write();
            }

            void check_keep_alive() {
                log_trace;
                auto req_conn_hdr = req_.get_header_value("connection");
                if(req_.is_http11()) {
                    keep_alive_ = req_conn_hdr.empty() || !iequal(req_conn_hdr.data(), req_conn_hdr.length(), "close");
                }
                else {
                    keep_alive_ = !req_conn_hdr.empty() && iequal(req_conn_hdr.data(), req_conn_hdr.length(), "keep-alive");
                }

                if(keep_alive_) {
                    res_.add_header("Connection", "keep-alive");
                }
                else {
                    res_.add_header("Connection", "close");
                }
            }
            void response_back(status_type status) {
                res_.set_status_and_content(status);
                do_write();
            }

            void response_back(status_type status, std::string&& content) {
                res_.set_status_and_content(status, std::move(content));
            }

            void handle_body() {
                log_trace;
                req_.set_body(socket_->read_all());
                if(!handle_gzip()){
                    response_back(status_type::bad_request, "gzip uncompress error");
                    return;
                }

                if(req_.is_complete()){
                    call_back();
                    do_write();
                }
            }

            bool handle_gzip() {
                if(req_.has_gzip()) {
                    return req_.uncompress();
                }
                return true;
            }

            void call_back() {
                log_trace;
                assert(http_handler_);
                http_handler_(req_, res_);
            }

            void do_write() {
                log_debug(res_.to_string());
                socket_->write(res_.to_string()).then([this]{
                    log_trace;
                    if(!keep_alive_) {
                        socket_->close();
                    }
                });
            }

        private:
            http_handler http_handler_;
            request req_;
            response res_;

            bool keep_alive_;
    };
}
