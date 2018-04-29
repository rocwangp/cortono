#pragma once

#include "socket.hpp"
#include "eventloop.hpp"
#include "../util/util.hpp"

namespace cortono::net
{
    class Acceptor
    {
        public:

#ifdef CORTONO_USE_SSL
            using ConnCallBack = std::function<void(int, SSL*)>;
#else
            using ConnCallBack = std::function<void(int)>;
#endif
            Acceptor(EventLoop* loop, std::string_view ip, unsigned short port)
                : idle_fd_(util::io::open("dev/null")),
                  loop_(loop)
            {
                socket_.tie(loop_->poller());
                socket_.set_option(TcpSocket::reuse_addr, TcpSocket::reuse_port, TcpSocket::non_block);
                socket_.bind(ip, port);
                socket_.set_read_callback(std::bind(&Acceptor::handle_accept, this));
            }
            ~Acceptor()
            {
                util::io::close(idle_fd_);
            }
            void start() {
                socket_.enable_reading();
                socket_.listen();
            }
            void on_connection(ConnCallBack cb) {
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
#ifdef CORTONO_USE_SSL
                    SSL* ssl = ip::tcp::ssl::new_ssl_and_set_fd(fd);
                    ip::tcp::ssl::accept(ssl);
                    if(conn_cb_) {
                        conn_cb_(fd, ssl);
                    }
                    else {
                        ip::tcp::sockets::close(fd);
                        ip::tcp::ssl::close(ssl);
                    }
#else
                    if(conn_cb_) {
                        conn_cb_(fd);
                    }
                    else {
                        ip::tcp::sockets::close(fd);
                        break;
                    }
#endif
                }
            }
        private:
            int idle_fd_;
            EventLoop* loop_;
            TcpSocket socket_;
            ConnCallBack conn_cb_;
    };
}
