#pragma once

#include "../cortono.hpp"
#include "http_module.hpp"
#include "http_request.hpp"
#include "http_response.hpp"

#include <memory>
#include <vector>
#include <regex>
#include <string_view>

namespace cortono::http
{
    using namespace cortono::net;
    using namespace std::literals;


    class http_module;
    class http_session;

    class http_server
    {
        public:
            http_server(std::string_view ip, unsigned short port)
                : service_(&base_, ip, port)
            {
                service_.on_conn([this](auto socket) {
                    socket->enable_option(cort_socket::NO_DELAY);
                    service_.register_session(socket, std::make_shared<http_session>(socket, modules_));
                });
            }

            void register_module(std::shared_ptr<http_module> module) {
                modules_.emplace_back(module);
            }

            void start() {
                service_.start();
                base_.sync_loop();
            }

        private:
            cort_eventloop base_;
            cort_service<http_session> service_;
            std::vector<std::shared_ptr<http_module>> modules_;
    };


    class http_session : public cort_session
    {
        public:
            http_session(std::shared_ptr<cort_socket> socket,
                         std::vector<std::shared_ptr<http_module>>& modules)
                : cort_session(socket),
                  modules_(modules)
            {
            }

            void on_read() {
                log_trace;
                do_read();
            }
        private:
            void do_read() {
                switch(auto status = request_.get_parse_status()) {
                    case parse_status::parse_line:
                        do_parse_line();
                        break;
                    case parse_status::parse_header:
                        do_parse_header();
                        break;
                    case parse_status::parse_body:
                        do_parse_body();
                        break;
                    default:
                        do_discard();
                        break;
                }
            }

            void do_parse_line() {
                if(auto status = request_.parse_line(socket_->read_buffer());
                        status == parse_status::parse_error) {
                    log_error("parse line error");
                    response_back(status_type::bad_request);
                }
                else {
                    do_parse_header();
                }
            }

            void do_parse_header() {
                log_trace;
                auto read_buffer = socket_->read_buffer();
                while(true) {
                    switch(auto status = request_.parse_header(read_buffer)) {
                        case parse_status::parse_body:
                            do_parse_body();
                            return;
                        case parse_status::parse_done:
                            do_handle_request();
                            return;
                        case parse_status::parse_error:
                            log_error("parse header error");
                            response_back(status_type::bad_request);
                            return;
                        default:
                            ;
                    }
                }
            }

            void do_parse_body() {
                log_trace;
                if(request_.parse_body(socket_->read_buffer()) == parse_status::parse_done) {
                    do_handle_request();
                }
            }

            void do_discard() {
                socket_->read_buffer()->clear();
            }

            void do_handle_request() {
                request_.set_body(socket_->read_buffer()->to_string_view());
                for(auto &module : modules_) {
                    if(auto status = module->handle(request_, response_, socket_);
                            status == module_handle_status::interval_redirect) {
                        response_.reset();
                        do_handle_request();
                        return;
                    }
                    else if(status == module_handle_status::error) {
                        response_back(status_type::bad_request);
                        return;
                    }
                }
                if(!request_.keep_alive()) {
                    do_close();
                }
            }

            void do_write() {
                socket_->write(response_.to_string());
            }

            void do_close() {
                if(socket_->write_done()) {
                    log_info("close connection");
                    socket_->close();
                }
                else {
                    socket_->invoke_after_write_done([socket = socket_]{
                        log_info("close connection");
                        socket->close();
                    });
                }
            }

            void response_back(status_type status) {
                response_.set_status_and_content(status);
                do_write();
                do_close();
            }

        private:
            std::vector<std::shared_ptr<http_module>>& modules_;
            parse_status status_;

            http_request request_;
            http_response response_;
    };
}
