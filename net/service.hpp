#pragma once

#include "acceptor.hpp"
#include "socket.hpp"
#include "eventloop.hpp"
#include "session.hpp"
#include "connection.hpp"
#include "../std.hpp"
#include "../util/noncopyable.hpp"
#include "../util/threadpool.hpp"

namespace cortono::net
{
    class TcpService : private util::noncopyable
    {
        public:
            typedef std::function<void(TcpConnection::Pointer)> ConnCallBack;
            typedef TcpConnection::MessageCallBack  MessageCallBack;
            typedef TcpConnection::ErrorCallBack    ErrorCallBack;
            typedef TcpConnection::CloseCallBack    CloseCallBack;

            TcpService(EventLoop* loop, std::string_view ip, unsigned short port)
                : loop_(loop),
                  acceptor_(loop, ip, port)
            {
#ifdef CORTONO_USE_SSL
                acceptor_.on_connection([this](int fd, SSL* ssl) {
#else
                acceptor_.on_connection([this](int fd) {
#endif
                    auto loop = eventloops_.empty()
                                    ? loop_
                                    : eventloops_[(++loop_idx_) % eventloops_.size()];
#ifdef CORTONO_USE_SSL
                    auto conn = std::make_shared<TcpConnection>(loop, fd, ssl);
#else
                    auto conn = std::make_shared<TcpConnection>(loop, fd);
#endif
                    conn->on_read([this](const auto& c) {
                        if(msg_cb_) { msg_cb_(c); }
                    });
                    conn->on_error([this](const auto& c) {
                        if(error_cb_) { error_cb_(c); }
                    });
                    conn->on_close([this](const auto& c) {
                        /* if(close_cb_) { close_cb_(c); } */
                        remove_connection(c);
                    });
                    connections_[conn->name()] = conn;
                    if(conn_cb_) { conn_cb_(conn); }
                });
            }

            ~TcpService() {
                for(auto loop : eventloops_) {
                    loop->quit();
                }
            }
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
            void start(int thread_nums = std::thread::hardware_concurrency()) {
#ifdef CORTONO_USE_SSL
                ip::tcp::ssl::init_ssl(CA_CERT_FILE, SERVER_CERT_FILE, SERVER_KEY_FILE);
#endif
                while(thread_nums--) {
                    util::threadpool::instance().async([this] {
                        EventLoop loop;
                        {
                            std::unique_lock lock{ mutex_ };
                            eventloops_.emplace_back(&loop);
                        }
                        loop.loop();
                    });
                }
                util::threadpool::instance().start();
                acceptor_.start();
            }
        private:
            void remove_connection(const TcpConnection::Pointer& conn) {
                /* loop_->safe_call([this, name = conn->name()]{ */
                /*     log_info("remove connection"); */
                std::unique_lock lock { mutex_ };
                connections_.erase(conn->name());
                /* }); */
            }
        private:
            EventLoop *loop_ = nullptr;
            Acceptor acceptor_;
            int loop_idx_ = -1;
            std::mutex mutex_;
            std::vector<EventLoop*> eventloops_;
            std::unordered_map<std::string, TcpConnection::Pointer> connections_;
            ConnCallBack conn_cb_ = nullptr;
            MessageCallBack msg_cb_ = nullptr;
            ErrorCallBack error_cb_ = nullptr;
            CloseCallBack close_cb_ = nullptr;
    };
}
