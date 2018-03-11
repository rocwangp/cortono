#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_module.hpp"
#include "http_request.hpp"
#include "http_response.hpp"

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
                    socket->enable_option(tcp_socket::no_delay);
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
            event_loop base_;
            tcp_service<http_session> service_;
            std::vector<std::shared_ptr<http_module>> modules_;
    };


    class http_session : public tcp_session
    {
        public:
            http_session(std::shared_ptr<tcp_socket> socket,
                         std::vector<std::shared_ptr<http_module>>& modules)
                : tcp_session(socket),
                  modules_(modules)
            {
                set_read_callback(std::bind(&http_session::parse_line, this));
            }
        private:
            void parse_line() {
                if(auto status = request_.parse_line(socket_->read_buffer());
                        status == parse_status::parse_error) {
                    log_error("parse line error");
                    response_back(status_type::bad_request);
                }
                else {
                    set_read_callback(std::bind(&http_session::parse_header, this));
                    parse_header();
                }
            }
            void parse_header() {
                log_trace;
                auto read_buffer = socket_->read_buffer();
                while(true) {
                    switch(auto status = request_.parse_header(read_buffer)) {
                        case parse_status::parse_body:
                            set_read_callback(std::bind(&http_session::parse_body, this));
                            parse_body();
                            return;
                        case parse_status::parse_done:
                            set_read_callback(std::bind(&http_session::do_none, this));
                            handle_request();
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
            void do_none() {

            }
            void parse_body() {
                log_trace;
                if(request_.parse_body(socket_->read_buffer()) == parse_status::parse_done) {
                    handle_request();
                }
            }
            void discard() {
                socket_->read_buffer()->clear();
            }
            void handle_request() {
                request_.set_body(socket_->read_buffer()->to_string_view());
                for(auto &module : modules_) {
                    if(auto status = module->handle(request_, response_, socket_);
                            status == module_handle_status::redirection) {
                        response_.reset();
                        handle_request();
                        return;
                    }
                    else if(status == module_handle_status::error) {
                        response_back(status_type::bad_request);
                        return;
                    }
                    else if(status == module_handle_status::done) {
                        do_close();
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
