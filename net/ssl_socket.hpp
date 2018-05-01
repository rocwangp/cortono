#pragma once

#include "socket.hpp"

namespace cortono::net
{
#ifdef CORTONO_USE_SSL
    class SslSocket : public TcpSocket
    {
        public:
            using ssl_sockets = ip::tcp::ssl;
            SslSocket()
                : TcpSocket(),
                  ssl_(ssl_sockets::new_ssl_and_set_fd(fd_))
            {
            }
            SslSocket(int fd)
                : TcpSocket(fd),
                  ssl_(ssl_sockets::new_ssl_and_set_fd(fd_))
            {
            }
            SslSocket(int fd, ::SSL* ssl)
                : TcpSocket(fd),
                  ssl_(ssl)
            {
            }

            ~SslSocket() {
                ssl_sockets::close(ssl_);
            }
            void close() {
                TcpSocket::close();
                ssl_sockets::close(ssl_);
            }
            bool handshake() {
                struct pollfd pfd;
                pfd.fd = fd_;
                pfd.events = POLLOUT | POLLERR;
                if(::poll(&pfd, 1, 0) == 1 && ip::tcp::sockets::get_error(fd_) == 0) {
                    // TODO: 采用非阻塞
                    set_option(TcpSocket::block);
                    bool ret = ip::tcp::ssl::connect(ssl_);
                    set_option(TcpSocket::non_block);
                    if(ret) {
                        return true;
                    }
                }
                return false;
            }

            SSL* ssl() {
                return ssl_;
            }
            int send(const char* buffer, int len) {
                return ssl_sockets::send(ssl_, buffer, len);
            }
            int recv(char* buffer, int len) {
                return ssl_sockets::recv(ssl_, buffer, len);
            }
        protected:
            ::SSL* ssl_;
    };
#endif
}
