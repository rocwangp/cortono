#pragma once

#include <cassert>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <cerrno>

#include "socket.hpp"
#include "session.hpp"
#include "poller.hpp"
#include "../util/util.hpp"
#include "../util/noncopyable.hpp"
#include "../util/threadpool.hpp"

namespace cortono::net
{
    template <typename session_type>
    class cort_hsha : private util::noncopyable
    {
        public:
            typedef std::function<void(std::shared_ptr<cort_socket>)> callback_type;

            cort_hsha(const std::string& ip = "localhost", unsigned short port = 9999)
                : poller_(std::make_shared<cort_poller>()),
                  acceptor_(std::make_shared<cort_socket>())
            {
                acceptor_->tie(poller_);
                acceptor_->enable_option(cort_socket::REUSE_ADDR, cort_socket::REUSE_POST, cort_socket::NON_BLOCK);
                acceptor_->enable_read(std::bind(&cort_hsha::handle_accept, this));
                util::exitif(!acceptor_->bind(ip, port), "bind error");
                util::exitif(!acceptor_->listen(), "listen error");
            }
            ~cort_hsha() {}

            void start() {
                util::threadpool::instance().start();
                while(true) {
                    poller_->wait();
                }
            }

            void on_conn(callback_type cb){
                conn_cb_ = std::move(cb);
            }

            void register_session(std::shared_ptr<cort_socket> socket,
                                  std::shared_ptr<session_type> session) {
                sessions_[socket.get()] = session;
            }
        private:
            void handle_accept() {
                while(true) {
                    int fd = acceptor_->accept();
                    if(fd == -1)
                        return;
                    auto socket = std::make_shared<cort_socket>(fd);
                    socket->tie(poller_);
                    socket->enable_option(cort_socket::REUSE_ADDR, cort_socket::REUSE_POST, cort_socket::NON_BLOCK);
                    std::weak_ptr weak_socket { socket };
                    socket->enable_read([this, weak_socket] {
                        util::threadpool::instance().async([this, weak_socket] {
                            if(auto socket = weak_socket.lock(); socket) {
                                socket->recv_to_buffer();
                                sessions_[socket.get()]->on_read();
                            }
                        });
                    });

                    socket->enable_close([this, weak_socket] {
                        util::threadpool::instance().async([this, weak_socket] {
                            if(auto socket = weak_socket.lock(); socket) {
                                socket->disable_all();
                                sessions_[socket.get()]->on_close();
                                {
                                    std::unique_lock lock { mutex_ };
                                    sessions_.erase(socket.get());
                                }
                            }
                        });
                    });
                    if(conn_cb_) {
                        conn_cb_(socket);
                    }
                }
            }
        private:
            callback_type conn_cb_ = nullptr;
            std::shared_ptr<cort_poller> poller_;
            std::shared_ptr<cort_socket> acceptor_;
            std::unordered_map<cort_socket*, std::shared_ptr<session_type>> sessions_;
            std::mutex mutex_;
    };
}
