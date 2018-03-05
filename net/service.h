#pragma once

#include <thread>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "socket.h"
#include "eventloop.h"
#include "session.h"
#include "../util/noncopyable.h"
#include "../util/threadpool.h"

namespace cortono::net
{
    template <typename session_type>
    class cort_service : private util::noncopyable
    {
        public:
            typedef std::function<void(std::shared_ptr<cort_socket>)> callback_type;

            cort_service(cort_eventloop* loop, std::string_view ip, unsigned short port)
                : loop_(loop),
                  loop_idx_(-1),
                  acceptor_(std::make_unique<cort_socket>())
            {
                acceptor_->tie(loop_->poller());
                acceptor_->enable_option(cort_socket::REUSE_ADDR, cort_socket::REUSE_POST, cort_socket::NON_BLOCK);
                acceptor_->enable_read(std::bind(&cort_service::handle_accept, this));
                util::exitif(!acceptor_->bind(ip, port), "fail to bind <", ip, port, ">", std::strerror(errno));
                util::exitif(!acceptor_->listen(), "fail to listen");
            }

            ~cort_service()
            {

            }

        public:
            void start(int thread_nums = std::thread::hardware_concurrency()) {
                log_debug(thread_nums);
                while(thread_nums--) {
                    util::threadpool::instance().async([this] {
                        auto loop_ptr = std::make_shared<cort_eventloop>();
                        event_loops_.push_back(loop_ptr);
                        loop_ptr->sync_loop();
                    });
                }
                util::threadpool::instance().start();
            }

            void on_read(callback_type cb) {
                read_cb_ = cb;
            }
            void on_conn(callback_type cb) {
                conn_cb_ = cb;
            }
            void on_write(callback_type cb) {
                write_cb_ = cb;
            }


            void register_session(std::shared_ptr<cort_socket> socket, std::shared_ptr<session_type> session) {
                sessions_[socket.get()] = session;
            }

            std::shared_ptr<session_type> session(std::shared_ptr<cort_socket> socket) {
                if(auto it = sessions_.find(socket.get()); it != sessions_.end()) {
                    return sessions_[socket.get()];
                }
                else {
                    log_fatal("no socket in service");
                    return nullptr;
                }
            }

        private:
            void handle_accept() {
                log_trace;
                while(true) {
                    int fd = acceptor_->accept();
                    log_debug(fd);
                    if(fd == -1)
                        return;
                    auto loop = event_loops_.empty()
                                    ? loop_
                                    : event_loops_[(++loop_idx_) % event_loops_.size()].get();
                    loop->safe_call([this, loop, fd]{
                        auto socket = std::make_shared<cort_socket>(fd);
                        socket->tie(loop->poller());
                        socket->enable_option(cort_socket::NON_BLOCK);
                        std::weak_ptr weak_socket { socket };
                        socket->enable_read([this, weak_socket] {
                            log_trace;
                            if(auto strong_socket = weak_socket.lock(); strong_socket) {
                                if(strong_socket->recv_to_buffer()) {
                                    sessions_[strong_socket.get()]->on_read();
                                }
                            }
                        });
                        socket->enable_close([this, weak_socket] {
                            log_trace;
                            if(auto strong_socket = weak_socket.lock(); strong_socket) {
                                strong_socket->disable_all();
                                sessions_[strong_socket.get()]->on_close();
                                {
                                    std::unique_lock lock { mutex_ };
                                    sessions_.erase(strong_socket.get());
                                }
                            }
                        });
                        if(conn_cb_) {
                            conn_cb_(socket);
                        }
                    });
                }
            }

        private:
            callback_type conn_cb_, read_cb_, write_cb_, close_cb_;

            cort_eventloop *loop_;
            int loop_idx_;
            std::mutex mutex_;
            std::unique_ptr<cort_socket> acceptor_;
            std::vector<std::shared_ptr<cort_eventloop>> event_loops_;
            std::unordered_map<cort_socket*, std::shared_ptr<session_type>> sessions_;

    };
}
