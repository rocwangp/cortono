#pragma once

#include "socket.hpp"

namespace cortono::net
{
#ifdef CORTONO_USE_SSL
    class SSLSocket : public TcpSocket
    {
        public:
            using sockets = ip::tcp::sockets;
            using ssl_sockets = ip::tcp::ssl;
            SSLSocket()
                : TcpSocket(),
                  ssl_(ssl_sockets::new_ssl_and_set_fd(fd_))
            {
            }
            SSLSocket(int fd)
                : TcpSocket(fd),
                  ssl_(ssl_sockets::new_ssl_and_set_fd(fd_))
            {
            }
            SSLSocket(int fd, ::SSL* ssl)
                : TcpSocket(fd),
                  ssl_(ssl)
            {
            }

            ~SSLSocket() {
                ssl_sockets::close(ssl_);
            }
            void close() {
                TcpSocket::close();
                ssl_sockets::close(ssl_);
            }
            SSL* ssl() {
                return ssl_;
            }
            SSL* accept() {
                int fd = TcpSocket::accept();
                if(fd != -1) {
                    ::SSL* ssl = ssl_sockets::new_ssl_and_set_fd(fd);
                    if(ssl_sockets::accept(ssl)) {
                        return ssl;
                    }
                    sockets::close(fd);
                    ssl_sockets::close(ssl);
                }
                return nullptr;
            }
        protected:
            ::SSL* ssl_;
    };
#endif
}
