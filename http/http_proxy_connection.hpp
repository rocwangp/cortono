#pragma once

#include "http_parser.hpp"
#include "http_response.hpp"

#include <iostream>

namespace cortono::http
{
    template <typename Handler, typename Client, typename Connection>
    class WebProxyConnection
    {
        public:
            WebProxyConnection(Handler& handler)
                : handler_(handler)
            {  }

            void handle_read(typename Connection::Pointer& conn_ptr) {
                log_debug(conn_ptr->recv_buffer()->to_string());
                int len = parser_.feed(conn_ptr->recv_buffer()->data() + parse_len_,
                                       conn_ptr->recv_buffer()->size() - parse_len_);
                parse_len_ += len;
                if(parser_.done()) {
                    req_ = std::move(parser_.to_request());
                    bool keep_alive = true;
                    if(req_.has_header("Proxy-Connection")) {
                        if(utils::iequal(req_.get_header_value("Connection"), "Close")) {
                            keep_alive = false;
                        }
                    }

                    std::string ip;
                    unsigned short port{ 0 };
                    if(req_.has_header("Host")) {
                        ip = req_.get_header_value("Host");
                        if(auto pos = ip.find_first_of(':'); pos != std::string::npos) {
                            port = std::strtoul(ip.substr(pos + 1).data(), nullptr, 10);
                            ip.resize(pos);
                        }
                        else {
#ifdef CORTONO_USE_SSL
                            log_info("defined CORTONO_USE_SSL, use port 443");
                            if(std::is_same_v<typename Client::ClientConnType, Connection>) {
                                port = 443;
                            }
                            else {
                                port = 80;
                            }
#else
                            port = 80;
#endif
                        }
                    }
                    else {
                        conn_ptr->send("HTTP/1.1 404 Not Found");
                        conn_ptr->close();
                        return;
                    }
                    log_debug(ip, port);
                    std::weak_ptr client_weak_conn{ conn_ptr };
                    auto read_cb = [client_weak_conn](auto proxy_conn_ptr) {
                        if(auto strong_conn = client_weak_conn.lock(); strong_conn) {
                            strong_conn->send(proxy_conn_ptr->recv_all());
                        }
                        else {
                            proxy_conn_ptr->close();
                        }
                    };
                    auto close_cb = [keep_alive, client_weak_conn](auto) {
                        if(auto strong_conn = client_weak_conn.lock(); strong_conn) {
                            if(!keep_alive) {
                                log_info("close client connection");
                                strong_conn->close();
                            }
                        }
                    };
                    auto proxy_conn = Client::connect(conn_ptr->loop(), ip, port, read_cb, nullptr, close_cb);
                    if(proxy_conn) {
                        conn_ptr->on_close([proxy_conn](auto) {
                            proxy_conn->close();
                        });
                        // 如果首次是CONNECT请求，需要在连接到目标服务器之后返回”连接成功“信息
                        if(req_.method == HttpMethod::CONNECT) {
                            conn_ptr->clear_recv_buffer();
                            conn_ptr->send("HTTP/1.1 200 Connection Established\r\n\r\n");
                        }
                        // 如果首次是GET请求，直接转发
                        else if(req_.method == HttpMethod::GET) {
                            proxy_conn->send(conn_ptr->recv_all());
                        }
                        // TODO: 支持其它请求类型
                        else {
                            conn_ptr->send("HTTP/1.1 500 Internal Server Error");
                            conn_ptr->close();
                        }
                        // 处理完首次请求后重置可读回调，直接转发来往数据
                        // **********注意*************
                        // 当前WebProxyConnection对象唯一的一个智能指针保存在conn_ptr的on_read里面
                        // 一旦conn_ptr的可读回调被更改，WebProxyConnection对象也就会被析构
                        // 所以末尾的两行注释如果在conn_ptr->on_read(...)之后执行就会出错
                        conn_ptr->on_read([proxy_conn = std::move(proxy_conn)](auto conn_ptr) {
                            proxy_conn->send(conn_ptr->recv_all());
                        });
                    }
                    else {
                        conn_ptr->send("HTTP/1.1 500 Internal Server Error");
                        conn_ptr->close();
                    }
                    /* *********Boom********* */
                    /* parse_len_ = 0; */
                    /* parser_.clear(); */
                }
            }
        private:
            Handler& handler_;
            HttpParser parser_;
            Request req_;
            Response res_;
            std::size_t parse_len_{ 0 };
    };
}
