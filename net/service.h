#pragma once

#include <thread>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "socket.h"
#include "eventloop.h"
#include "../util/threadpool.h"

namespace cortono
{
    namespace net
    {
        template <typename Session>
        class Service
        {
            public:
                Service(EventLoop* loop, const std::string& ip, unsigned short port)
                    : loop_(loop),
                      loop_idx_(-1),
                      acceptor_(std::make_unique<Socket>())
                {
                    acceptor_->tie(loop_->poller());
                    acceptor_->enable_option(Socket::REUSE_ADDR, Socket::REUSE_POST, Socket::NON_BLOCK);
                    acceptor_->enable_read(std::bind(&Service::handle_accept, this));
                    util::exitif(!acceptor_->bind(ip, port), "fail to bind <", ip, port, ">", std::strerror(errno));
                    util::exitif(!acceptor_->listen(), "fail to listen");
                }

                ~Service()
                {

                }

            public:
                void start(int thread_nums = std::thread::hardware_concurrency())
                {
                    log_debug(thread_nums);
                    while(thread_nums--)
                    {
                        util::ThreadPool::Instance().async(
                                [this]
                                {
                                    auto loop_ptr = std::make_shared<EventLoop>();
                                    event_loops_.push_back(loop_ptr);
                                    loop_ptr->sync_loop();
                                }
                            );
                    }
                    util::ThreadPool::Instance().start();
                }

            private:
                void handle_accept()
                {
                    log_trace;
                    while(true)
                    {
                        int fd = acceptor_->accept();
                        if(fd == -1)
                            return;
                        auto loop = event_loops_.empty()
                                        ? loop_
                                        : event_loops_[(++loop_idx_) % event_loops_.size()].get();
                        auto socket = std::make_shared<Socket>(fd);
                        auto session = std::make_shared<Session>(socket);
                        socket->tie(loop->poller());
                        socket->enable_option(Socket::NON_BLOCK);
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
                                        socket->recv_to_buffer();
                                        strong_session->on_recv(socket);
                                    }
                                }
                            );
                        socket->enable_close(
                                [socket, weak_session, this]
                                {
                                    if(auto strong_session = weak_session.lock(); strong_session)
                                    {
                                        {
                                            std::unique_lock<std::mutex> lock(mutex_);
                                            sessions_.erase(socket.get());
                                        }
                                        socket->disable_all();
                                        strong_session->on_close();
                                    }
                                }
                            );

                    }
                }

            private:
                EventLoop *loop_;
                int loop_idx_;
                std::mutex mutex_;
                std::unique_ptr<Socket> acceptor_;
                std::vector<std::shared_ptr<EventLoop>> event_loops_;
                std::unordered_map<Socket*, std::shared_ptr<Session>> sessions_;
        };
    }
}
