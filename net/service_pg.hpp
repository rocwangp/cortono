#pragma once

#include "../std.hpp"
#include "../ip/sockets.hpp"
#include "socket.hpp"
#include "eventloop.hpp"

namespace cortono::net
{
    using namespace cortono::ip::tcp;

    template <typename session_type>
    class service_pg
    {
        public:
            service_pg(std::string_view ip, unsigned short port)
                : listen_fd_(sockets::nonblock_socket()),
                  ip_(ip.data(), ip.length()),
                  port_(port) {
            }
            ~service_pg() {
                for(auto pid : pids_) {
                    waitpid(pid, nullptr, 0);
                }
            }
            void start(int process_nums = 16) {
                while(process_nums--) {
                    pid_t pid = ::fork();
                    if(pid == 0) {
                        loop_ = std::make_shared<event_loop>();
                        acceptor_ = std::make_shared<tcp_socket>(listen_fd_);
                        acceptor_->tie(loop_->poller());
                        acceptor_->enable_option(tcp_socket::reuse_addr, tcp_socket::reuse_port);
                        acceptor_->enable_read(std::bind(&service_pg::handle_accept, this));
                        acceptor_->bind_and_listen(ip_, port_);
                        process_eventloop();
                    }
                    else {
                        pids_.emplace_back(pid);
                    }
                }
                sockets::close(listen_fd_);
            }
            void on_conn(std::function<void(std::shared_ptr<tcp_socket>)> cb) {
                conn_cb_ = std::move(cb);
            }
            void register_session(std::shared_ptr<tcp_socket> socket, std::shared_ptr<session_type> session) {
                sessions_.emplace(socket.get(), session);
            }
        private:
            void handle_accept() {
                while(true) {
                    int fd = acceptor_->accept();
                    if(fd == -1) {
                        return;
                    }
                    auto socket = std::make_shared<tcp_socket>(fd);
                    socket->tie(loop_->poller());
                    socket->enable_option(tcp_socket::non_block);
                    std::weak_ptr<tcp_socket> weak_socket = socket;
                    socket->enable_read([this, weak_socket]{
                        if(auto strong_socket = weak_socket.lock(); strong_socket) {
                            if(strong_socket->recv_to_buffer()) {
                                sessions_[strong_socket.get()]->on_read();
                            }
                        }
                    });
                    socket->enable_close([this, socket]{
                        sessions_.erase(socket.get());
                    });

                    util::exitif(conn_cb_ == nullptr, "conn callback is nullptr");
                    conn_cb_(socket);
                }
            }
            void process_eventloop() {
                while(true) {

                }
            }

        private:
            int max_conn_nums_ = 100000;
            int conn_nums_;
            int listen_fd_;
            unsigned short port_;
            std::string ip_;
            std::vector<pid_t> pids_;
            std::shared_ptr<event_loop> loop_;
            std::shared_ptr<tcp_socket> acceptor_;
            std::unordered_map<tcp_socket*, session_type> sessions_;
            std::function<void(std::shared_ptr<tcp_socket>)> conn_cb_;
    };
}
