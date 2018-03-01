#pragma once

#include <cassert>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <cerrno>

#include "socket.h"
#include "poller.h"
#include "session.h"
#include "../util/util.h"
#include "../util/threadpool.h"

namespace cortono
{
    namespace net
    {
        template <class Session>
        class HSHA
        {
            public:
                HSHA(const std::string& ip = "localhost", unsigned short port = 9999)
                    : poller_(std::make_shared<Poller>()),
                      acceptor_(std::make_shared<Socket>())
                {
                    acceptor_->tie(poller_);
                    acceptor_->enable_option(Socket::REUSE_ADDR, Socket::REUSE_POST, Socket::NON_BLOCK);
                    acceptor_->enable_read(std::bind(&HSHA::handle_accept, this));
                    util::exitif(!acceptor_->bind(ip, port), "bind error");
                    util::exitif(!acceptor_->listen(), "listen error");
                }
                ~HSHA() {}

                void start()
                {
                    util::ThreadPool::Instance().start();
                    while(true)
                    {
                        poller_->wait();
                    }
                }

            private:
                void handle_accept()
                {
                    while(true)
                    {
                        int fd = acceptor_->accept();
                        if(fd == -1)
                            return;
                        auto socket = std::make_shared<Socket>(fd);
                        auto session = std::make_shared<Session>(socket);
                        socket->tie(poller_);
                        socket->enable_option(Socket::REUSE_ADDR, Socket::REUSE_POST, Socket::NON_BLOCK);
                        {
                            std::unique_lock<std::mutex> lock(mutex_);
                            sessions_[socket.get()] = session;
                        }
                        std::weak_ptr<Session> weak_session = session;
                        socket->enable_read(
                                [socket, weak_session]
                                {
                                    if(auto strong_session = weak_session.lock(); strong_session)
                                    {
                                        util::ThreadPool::Instance().async(
                                                [socket, strong_session]
                                                {
                                                    socket->recv_to_buffer();
                                                    strong_session->on_recv(socket);
                                                }
                                            );
                                    }
                                }
                            );
                        socket->enable_close(
                                [weak_session, socket, this]
                                {
                                    if(weak_session.lock())
                                    {
                                        util::ThreadPool::Instance().async(
                                                [socket, this]
                                                {
                                                    {
                                                        std::unique_lock<std::mutex> lock(mutex_);
                                                        sessions_.erase(socket.get());
                                                    }
                                                    socket->disable_all();
                                                }
                                            );
                                    }
                                }
                            );
                    }
                }
            private:
                std::shared_ptr<Poller> poller_;
                std::shared_ptr<Socket> acceptor_;
                std::unordered_map<Socket*, std::shared_ptr<Session>> sessions_;
                std::mutex mutex_;
        };
    }
}
