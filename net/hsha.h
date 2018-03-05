#pragma once

#include <cassert>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <cerrno>

#include "socket.h"
#include "session.h"
#include "poller.h"
#include "../util/util.h"
#include "../util/noncopyable.h"
#include "../util/threadpool.h"

namespace cortono::net
{
    template <typename session_type>
    class cort_hsha : private util::noncopyable
    {
        public:
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

        private:
            void handle_accept() {
                while(true) {
                    int fd = acceptor_->accept();
                    if(fd == -1)
                        return;
                    auto socket = std::make_shared<cort_socket>(fd);
                    auto session = custom_session(socket);
                    socket->tie(poller_);
                    socket->enable_option(cort_socket::REUSE_ADDR, cort_socket::REUSE_POST, cort_socket::NON_BLOCK);
                    {
                        std::unique_lock lock { mutex_ };
                        sessions_[socket.get()] = session;
                    }
                    std::weak_ptr weak_session { session };
                    socket->enable_read([socket, weak_session] {
                        if(auto strong_session = weak_session.lock(); strong_session) {
                            util::threadpool::instance().async([socket, strong_session] {
                                socket->recv_to_buffer();
                                strong_session->on_read(socket);
                            });
                        }
                    });

                    socket->enable_close([weak_session, socket, this] {
                        if(weak_session.lock()) {
                            util::threadpool::instance().async([socket, this] {
                                {
                                    std::unique_lock lock { mutex_ };
                                    sessions_.erase(socket.get());
                                }
                                socket->disable_all();
                            });
                        }
                    });
                }
            }
        protected:
            auto custom_session(std::shared_ptr<cort_socket> socket) {
                return std::make_shared<session_type>(socket);
            }
        private:
            std::shared_ptr<cort_poller> poller_;
            std::shared_ptr<cort_socket> acceptor_;
            std::unordered_map<cort_socket*, std::shared_ptr<session_type>> sessions_;
            std::mutex mutex_;
    };
}
