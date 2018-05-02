#pragma once

#include "socket.hpp"
#include "eventloop.hpp"
#include "connection.hpp"
#include "../util/util.hpp"

namespace cortono::net
{
    class TcpAdaptor
    {
        public:
            using LoopProducer = std::function<EventLoop*()>;
            using ConnCallBack = std::function<void(TcpConnection::Pointer&&)>;

            TcpAdaptor(EventLoop* loop, std::string_view ip, unsigned short port)
                : idle_fd_(util::io::open("dev/null")),
                  loop_(loop)
            {
                socket_.tie(loop_->poller());
                socket_.set_option(TcpSocket::reuse_addr, TcpSocket::reuse_port, TcpSocket::non_block );
                socket_.bind(ip, port);
                socket_.set_read_callback(std::bind(&TcpAdaptor::handle_accept, this));
            }
            ~TcpAdaptor()
            {
                util::io::close(idle_fd_);
            }
            void start() {
                socket_.enable_reading();
                socket_.listen();
            }
            void on_connection(LoopProducer&& producer, ConnCallBack&& cb) {
                loop_producer_ = std::move(producer);
                conn_cb_ = std::move(cb);
            }
        protected:
            int accept_client() {
                int fd = socket_.accept();
                if(fd == -1) {
                    if(errno == EMFILE) {
                        util::io::close(idle_fd_);
                        fd = socket_.accept();
                        ip::tcp::sockets::close(fd);
                        idle_fd_ = util::io::open("dev/null");
                    }
                }
                return fd;
            }
        private:
            void handle_accept() {
                while(true) {
                    int fd = accept_client();
                    if(fd == -1) {
                        break;
                    }
                    if(loop_producer_ && conn_cb_) {
                        auto new_conn_ptr = std::make_shared<TcpConnection>(loop_producer_(), fd);
                        conn_cb_(std::move(new_conn_ptr));
                    }
                    else {
                        ip::tcp::sockets::close(fd);
                    }
                }
            }
        protected:
            int idle_fd_;
            EventLoop* loop_;
            TcpSocket socket_;
            LoopProducer loop_producer_;
        private:
            ConnCallBack conn_cb_;
    };
}
