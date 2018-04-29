#pragma once
#include "poller.hpp"
#include "connection.hpp"
#include "acceptor.hpp"
#include "eventloop.hpp"
#include "../std.hpp"
#include "../util/util.hpp"
#include "../util/noncopyable.hpp"
#include "../util/threadpool.hpp"

namespace cortono::net
{
    class HSHA : private util::noncopyable

    {
        public:
            typedef std::function<void(TcpConnection::Pointer)> ConnCallBack;
            typedef TcpConnection::MessageCallBack  MessageCallBack;
            typedef TcpConnection::ErrorCallBack    ErrorCallBack;
            typedef TcpConnection::CloseCallBack    CloseCallBack;

            HSHA(EventLoop* loop, std::string_view ip, unsigned short port)
                : loop_(loop),
                  acceptor_(loop, ip, port)
            {
#ifdef CORTONO_USE_SSL
                acceptor_.on_connection([this](int fd, SSL* ssl) {
                    auto conn = std::make_shared<TcpConnection>(loop_, fd, ssl);
#else
                acceptor_.on_connection([this](int fd) {
                    auto conn = std::make_shared<TcpConnection>(loop_, fd);
#endif
                    conn->on_read([this](auto c) {
                        if(msg_cb_) {
                            util::threadpool::instance().async([this, c]{
                                msg_cb_(c);
                            });
                        }
                    });
                    conn->on_error([this](auto c) {
                        if(error_cb_) {
                            util::threadpool::instance().async([this, c]{
                                error_cb_(c);
                                remove_connection(c);
                            });
                        }
                    });
                    conn->on_close([this](auto c) {
                        if(close_cb_) {
                            util::threadpool::instance().async([this, c]{
                                close_cb_(c);
                            });
                        }
                    });
                    {
                        std::unique_lock lock{mutex_};
                        connections_[conn->name()] = conn;
                    }
                    if(conn_cb_) { conn_cb_(conn); }
                });
            }

            ~HSHA() { }
            void on_conn(ConnCallBack cb) {
                conn_cb_ = std::move(cb);
            }
            void on_message(MessageCallBack cb) {
                msg_cb_ = std::move(cb);
            }
            void on_close(CloseCallBack cb) {
                close_cb_ = std::move(cb);
            }
            void on_error(ErrorCallBack cb) {
                error_cb_ = std::move(cb);
            }
        public:
            void start() {
                util::threadpool::instance().start();
                acceptor_.start();
            }
        private:
            void remove_connection(TcpConnection::Pointer conn) {
                std::unique_lock lock { mutex_ };
                connections_.erase(conn->name());
            }
        private:
            EventLoop *loop_ = nullptr;
            Acceptor acceptor_;
            std::mutex mutex_;
            std::unordered_map<std::string_view, TcpConnection::Pointer> connections_;
            ConnCallBack conn_cb_ = nullptr;
            MessageCallBack msg_cb_ = nullptr;
            ErrorCallBack error_cb_ = nullptr;
            CloseCallBack close_cb_ = nullptr;

    };
}
