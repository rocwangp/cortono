#pragma once

#include "socket.hpp"
#include "eventloop.hpp"
#include "session.hpp"
#include "../std.hpp"
#include "../util/noncopyable.hpp"
#include "../util/threadpool.hpp"

namespace cortono::net
{
    template <typename session_type>
    class tcp_service : private util::noncopyable
    {
        public:
            tcp_service(event_loop* loop, std::string_view ip, unsigned short port)
                : loop_(loop),
                  loop_idx_(-1),
                  acceptor_(std::make_unique<tcp_socket>())
            {
                acceptor_->tie(loop_->poller());
                acceptor_->enable_read(std::bind(&tcp_service::handle_accept, this));
                acceptor_->bind_and_listen(ip, port);
            }

            ~tcp_service()
            {

            }

        public:
            void start(int thread_nums = std::thread::hardware_concurrency()) {
                while(thread_nums--) {
                    util::threadpool::instance().async([this] {
                        auto loop_ptr = std::make_shared<event_loop>();
                        {
                            std::unique_lock lock{ mutex_ };
                            event_loops_.emplace_back(loop_ptr.get());
                        }
                        loop_ptr->sync_loop();
                    });
                }
                util::threadpool::instance().start();
            }

            /* 考虑到session_type构造函数的参数可以有多个，无法由内部网络库构造，
             * 同时为了便于对socket设置更多的选项
             * 采用外加服务器调用on_conn的方法构造session_type并调用register_session传递回cort_service
             * 使用方法如下：
             *
             * template <typename session_type>
             * class echo_server{
             *     public:
             *        echo_server(std::string_view ip, unsigned short port)
             *            : service_(ip, port)
             *        {
             *            service_.on_conn([this](auto socket) {
             *                socket->enable_option(...); # 自定义外加选项，如no_delay
             *                auto session = std::make_shared<session_type>(socket, ...); #可能存在多个参数
             *                service_.register_session(socket, session);
             *            });
             *        }
             *
             *        ...
             *
             *     private:
             *         event_loop base_;
             *         cort_service<session_type> service_;
             * }; */
            void on_conn(std::function<void(std::shared_ptr<tcp_socket>)> cb) {
                conn_cb_ = cb;
            }

            void register_session(std::shared_ptr<tcp_socket> socket, std::shared_ptr<session_type> session) {
                sessions_.emplace(socket.get(), session);
            }
        private:
            void handle_accept() {
                while(true) {
                    int fd = acceptor_->accept();
                    if(fd == -1)
                        return;
                    auto loop = event_loops_.empty()
                                    ? loop_
                                    : event_loops_[(++loop_idx_) % event_loops_.size()].get();
                    loop->safe_call([this, loop, fd]{
                        auto socket = std::make_shared<tcp_socket>(fd);
                        socket->tie(loop->poller());
                        socket->enable_option(tcp_socket::non_block);
                        std::weak_ptr weak_socket { socket };
                        socket->enable_read([this, weak_socket] {
                            if(auto strong_socket = weak_socket.lock(); strong_socket) {
                                if(strong_socket->recv_to_buffer()) {
                                    sessions_[strong_socket.get()]->on_read();
                                }
                            }
                        });
                        socket->enable_close([this, weak_socket] {
                            if(auto strong_socket = weak_socket.lock(); strong_socket) {
                                strong_socket->disable_all();
                                sessions_[strong_socket.get()]->on_close();
                                {
                                    std::unique_lock lock { mutex_ };
                                    sessions_.erase(strong_socket.get());
                                }
                            }
                        });
                        util::exitif(conn_cb_ == nullptr, "no conn callback");
                        conn_cb_(socket);
                    });
                }
            }

        private:

            event_loop *loop_;
            int loop_idx_;
            std::mutex mutex_;
            std::unique_ptr<tcp_socket> acceptor_;
            std::vector<std::shared_ptr<event_loop>> event_loops_;
            std::unordered_map<tcp_socket*, std::shared_ptr<session_type>> sessions_;
            std::function<void(std::shared_ptr<tcp_socket>)> conn_cb_;

    };
}
