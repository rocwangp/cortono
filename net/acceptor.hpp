#pragma once

#include "socket.hpp"
#include "eventloop.hpp"
#include "../util/util.hpp"

namespace cortono::net
{
    class Acceptor
    {
        public:
            Acceptor(EventLoop* loop, std::string_view ip, unsigned short port)
                : idle_fd_(util::io::open("dev/null")),
                  loop_(loop)
            {
                socket_.tie(loop_->poller());
                socket_.set_option(TcpSocket::reuse_addr, TcpSocket::reuse_port, TcpSocket::non_block);
                socket_.bind(ip, port);
                socket_.set_read_callback(std::bind(&Acceptor::handle_accept, this));
            }
            void start() {
                socket_.enable_reading();
                socket_.listen();
            }
            void on_connection(std::function<void(int)> cb) {
                conn_cb_ = std::move(cb);
            }
        private:
            void handle_accept() {
                while(true) {
                    int fd = ip::tcp::sockets::accept(socket_.fd());
                    if(fd == -1) {
                        if(errno == EMFILE) {
                            util::io::close(idle_fd_);
                            fd = ip::tcp::sockets::accept(socket_.fd());
                            ip::tcp::sockets::close(fd);
                            idle_fd_ = util::io::open("dev/null");
                        }
                        break;
                    }
                    else if(conn_cb_) {
                        conn_cb_(fd);
                    }
                    else {
                        ip::tcp::sockets::close(fd);
                        break;
                    }
                }
            }
        private:
            int idle_fd_;
            EventLoop* loop_;
            TcpSocket socket_;
            std::function<void(int)> conn_cb_;
    };
}
