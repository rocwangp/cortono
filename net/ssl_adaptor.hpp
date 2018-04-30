#pragma once

#include "../std.hpp"
#include "eventloop.hpp"
#include "adaptor.hpp"
#include "ssl_connection.hpp"

namespace cortono::net
{
#ifdef CORTONO_USE_SSL
    class SslAdaptor : public TcpAdaptor
    {
        public:
            using ConnCallBack = std::function<void(SslConnection::Pointer&&)>;

            SslAdaptor(EventLoop* loop, std::string_view ip, unsigned short port)
                : TcpAdaptor(loop, ip, port)
            {
                socket_.set_read_callback(std::bind(&SslAdaptor::handle_accept, this));
            }

            void on_connection(LoopProducer&& producer, ConnCallBack&& cb) {
                loop_producer_ = std::move(producer);
                conn_cb_ = std::move(cb);
            }
        private:
            void handle_accept() {
                while(true) {
                    int fd = accept_client();
                    if(fd == -1) {
                        break;
                    }
                    SSL* ssl = ip::tcp::ssl::new_ssl_and_set_fd(fd);
                    if(!ip::tcp::ssl::accept(ssl)) {
                        log_fatal("SSL_accept error");
                    }
                    if(loop_producer_ && conn_cb_) {
                        auto new_conn_ptr = std::make_shared<SslConnection>(loop_producer_(), fd, ssl);
                        conn_cb_(std::move(new_conn_ptr));
                    }
                    else {
                        ip::tcp::sockets::close(fd);
                        ip::tcp::ssl::close(ssl);
                    }
                }
            }
        private:
            ConnCallBack conn_cb_;
    };
#endif
}
