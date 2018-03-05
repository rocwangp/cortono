#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <error.h>

#include <string_view>
#include <string>
#include <cstring>

#include "../util/util.h"

namespace cortono::ip
{
    class address
    {
        public:
            static struct sockaddr to_sockaddr(std::string_view ip, unsigned short port) {
                struct sockaddr_in addr;
                std::memset(&addr, 0, sizeof(sockaddr));
                addr.sin_family = AF_INET;
                addr.sin_port = ::htons(port);
                std::string ip_addr { ip.data(), ip.length() };
                ::inet_pton(AF_INET, ip_addr.data(), &addr.sin_addr);
                struct sockaddr sockaddr;
                std::memmove(&sockaddr, &addr, sizeof(addr));
                return sockaddr;
            }

            static std::string local_address(int fd, bool ip4 = true) {
                struct sockaddr addr;
                std::memset(&addr, 0, sizeof(addr));
                ::getsockname(fd, &addr, nullptr);
                char ip[1024] = "\0";
                unsigned short port = 0;
                if(ip4) {
                    ::inet_ntop(AF_INET, &addr, ip, sizeof(ip));
                    port = ::ntohs(((struct sockaddr_in*)(&addr))->sin_port);
                }
                else {
                    ::inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
                    port = ::ntohs(((struct sockaddr_in6*)(&addr))->sin6_port);
                }
                return util::format("<%s:%u>", ip, port);
            }

            static std::string peer_address(int fd, bool ip4 = true) {
                struct sockaddr addr;
                std::memset(&addr, 0, sizeof(addr));
                ::getpeername(fd, &addr, nullptr);
                char ip[1024] = "\0";
                unsigned short port = 0;
                if(ip4) {
                    ::inet_ntop(AF_INET, &addr, ip, sizeof(ip));
                    port = ::ntohs(((struct sockaddr_in*)(&addr))->sin_port);
                }
                else {
                    ::inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
                    port = ::ntohs(((struct sockaddr_in6*)(&addr))->sin6_port);
                }
                return util::format("<%s:%u>", ip, port);
            }
    };


    namespace tcp
    {

        class sockets
        {
            public:
                static int nonblock_socket(bool ip4 = true) {
                    int sockfd = block_socket(ip4);
                    util::exitif(sockets::set_nonblock(sockfd), "fail to set nonblock");
                    return sockfd;
                }

                static int block_socket(bool ip4 = true) {
                    (void)(ip4);
                    return ::socket(AF_INET, SOCK_STREAM, 0);
                }

                static bool set_block(int sockfd) {
                    int flag = ::fcntl(sockfd, F_GETFL);
                    flag &= (~O_NONBLOCK);
                    return (::fcntl(sockfd, F_SETFL, flag) == 0) ? true : false;
;
                }
                static bool set_nonblock(int sockfd) {
                    int flag = ::fcntl(sockfd, F_GETFL);
                    flag |= O_NONBLOCK;
                    return (::fcntl(sockfd, F_SETFL, flag) == 0) ? true : false;
                }

                static bool bind(int fd, std::string_view ip, unsigned short port) {
                    struct sockaddr addr = ip::address::to_sockaddr(ip, port);
                    return (::bind(fd, &addr, sizeof(addr)) == 0) ? true : false;
                }

                static bool listen(int fd, long long int listen_num) {
                    return (::listen(fd, listen_num) == 0) ? true : false;
                }

                static int accept(int sockfd) {
                    int fd = ::accept(sockfd, nullptr, nullptr);
                    if(fd == -1) {
                        if(errno == EMFILE) {
                            util::io::close(idle_fd);
                            fd = ::accept(sockfd, nullptr, nullptr);
                            sockets::close(fd);
                            util::io::open("/dev/null");
                        }
                    }
                    return fd;
                }

                static bool connect(int fd, std::string_view ip, unsigned short port) {
                    struct sockaddr addr = ip::address::to_sockaddr(ip, port);
                    return (::connect(fd, &addr, sizeof(addr)) == 0) ? true : false;
                }

                static bool close(int fd) {
                    return (::close(fd) == 0) ? true : false;
                }

                static bool reuse_address(int fd) {
                    int val = 1;
                    return (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == 0)
                        ? true
                        : false;
                }

                static bool reuse_post(int fd) {
                    int val = 1;
                    return (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)) == 0)
                        ? true
                        : false;
                }

                static bool no_delay(int fd) {
                    int val = 1;
                    return (::setsockopt(fd, SOL_TCP, TCP_NODELAY, &val, sizeof(val)) == 0)
                        ? true
                        : false;
                }

                static int readable(int fd) {
                    int bytes = 0;
                    if(::ioctl(fd, FIONREAD, &bytes) == -1)
                        bytes = 0;
                    return bytes;
                }

                static int recv(int fd, char* buffer, int bytes) {
                    return ::recv(fd, buffer, bytes, MSG_NOSIGNAL);
                }

                static int send(int fd, const char* str, int len) {
                    return ::send(fd, str, len, MSG_NOSIGNAL);
                }

                static int send(int fd, const std::string& msg) {
                    return send(fd, msg.c_str(), msg.size());
                }


            private:
                static int idle_fd;
        };

        int sockets::idle_fd = util::io::open("/dev/null");
    }
}
