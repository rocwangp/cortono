#pragma once

#include "../ip/sockets.hpp"
#include "connection.hpp"
#include "ssl_socket.hpp"

namespace cortono::net
{
#ifdef CORTONO_USE_SSL
    class SSLConnection : public TcpConnection
    {
        public:
            using ssl_sockets = ip::tcp::ssl;

            SSLConnection(EventLoop* loop, int fd, ::SSL* ssl)
                : TcpConnection(loop, fd),
                  ssl_(ssl)
            {
            }
            ~SSLConnection() {
                ssl_sockets::close(ssl_);
            }

            virtual void send(const std::string& msg) override {
                if(msg.empty()) {
                    return;
                }

            }
        private:
            ::SSL* ssl_;
    };
#endif
}
