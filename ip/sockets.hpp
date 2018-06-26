#pragma once

#include "../std.hpp"
#include "../util/util.hpp"

namespace cortono::ip
{
    using namespace std::literals;
    class address
    {
        public:
            static struct sockaddr to_sockaddr(std::string_view ip, unsigned short port) {
                struct sockaddr_in addr;
                std::memset(&addr, 0, sizeof(sockaddr));
                addr.sin_family = AF_INET;
                addr.sin_port = htons(port);
                ::inet_pton(AF_INET, ip.data(), &addr.sin_addr);
                struct sockaddr sockaddr;
                std::memmove(&sockaddr, &addr, sizeof(addr));
                return sockaddr;
            }

            static std::string local_address(int fd, bool ip4 = true) {
                struct sockaddr addr;
                std::memset(&addr, 0, sizeof(addr));
                socklen_t len = sizeof(addr);
                ::getsockname(fd, &addr, &len);
                char ip[1024] = "\0";
                unsigned short port = 0;
                if(ip4) {
                    ::inet_ntop(AF_INET, &addr, ip, sizeof(ip));
                    port = ntohs(((struct sockaddr_in*)(&addr))->sin_port);
                }
                else {
                    ::inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
                    port = ntohs(((struct sockaddr_in6*)(&addr))->sin6_port);
                }
                return util::format("<%s:%u>", ip, port);
            }

            static std::string peer_address(int fd, bool ip4 = true) {
                struct sockaddr addr;
                std::memset(&addr, 0, sizeof(addr));
                socklen_t len = sizeof(addr);
                ::getpeername(fd, &addr, &len);
                char ip[1024] = "\0";
                unsigned short port = 0;
                if(ip4) {
                    inet_ntop(AF_INET, &addr, ip, sizeof(ip));
                    port = ntohs(((struct sockaddr_in*)(&addr))->sin_port);
                }
                else {
                    inet_ntop(AF_INET6, &addr, ip, sizeof(ip));
                    port = ntohs(((struct sockaddr_in6*)(&addr))->sin6_port);
                }
                return util::format("<%s:%u>", ip, port);
            }

            static std::string parse_ip_address(const std::string& host) {
                struct hostent *hptr = ::gethostbyname(host.c_str());
                if(hptr == nullptr) {
                    log_error("gethostbyname error...", host);
                    return host;
                }
                char buffer[1024] = "\0";
                ::inet_ntop(hptr->h_addrtype, hptr->h_addr_list[0], buffer, sizeof(buffer));
                return std::string(buffer);
            }

            static std::pair<std::string, unsigned short> parse_ip_port(struct sockaddr_in& sockaddr) {
                char ip[1024] = "\0";
                ::inet_ntop(AF_INET, &sockaddr.sin_addr, ip, sizeof(ip));
                unsigned short port = ntohs(sockaddr.sin_port);
                return { std::string{ ip }, port };
            }
    };

    namespace tcp
    {
        class sockets
        {
            public:
                static int nonblock_socket(bool ip4 = true) {
                    int sockfd = block_socket(ip4);
                    if(!sockets::set_nonblock(sockfd)) {
                        log_fatal("fail to set nonblock", sockfd, std::strerror(errno));
                    }
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
                    return ::accept(sockfd, nullptr, nullptr);
                }
                static bool connect(int fd, std::string_view ip, unsigned short port) {
                    struct sockaddr addr = ip::address::to_sockaddr(ip, port);
                    return (::connect(fd, &addr, sizeof(addr)) == 0) ? true : false;
                }
                static bool is_connecting() {
                    return errno == EINPROGRESS;
                }
                static bool is_connected(int fd) {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if(::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
                        return false;
                    }
                    else {
                        if(error == 0) {
                            return true;
                        }
                        else {
                            return false;
                        }
                    }
                }
                static int get_error(int fd) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
                    return err;
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
                static int sendfile(int fd, const std::string& filename, off_t offet, std::size_t count) {
                    int in_fd = util::io::open(filename);
                    if(in_fd == -1) {
                        log_error(strerror(errno));
                        return 0;
                    }
                    int n = ::sendfile(fd, in_fd, &offet, count);
                    if(n == -1) {
                        log_error(std::strerror(errno));
                    }
                    util::io::close(in_fd);
                    return n;
                    // sendfile的返回值可能小于指定的发送值，但是如果直接再次调用有可能因为缓冲区已满导致sendfile失败返回-1
                    // 所以改为connection控制，等到可写的时候再次调用sendfile
                    // if((std::size_t)n < count) {
                        // log_info("send file return", n, "bytes is less than filesize:", count, "call senfile again");
                        // return n + sendfile(fd, filename, offet + n, count - n);
                    // }
                    // else {
                        // return n;
                    // }
                }
        };

#ifdef CORTONO_USE_SSL

        class ssl
        {
            public:
                static bool init_ssl() {
                    ::SSLeay_add_ssl_algorithms();
                    ::OpenSSL_add_all_algorithms();
                    ::SSL_load_error_strings();
                    ::ERR_load_BIO_strings();
                    ::SSL_library_init();
                    // 使用SSL V3,V2
                    ssl_ctx = ::SSL_CTX_new(::SSLv23_method());
                    exitif(ssl_ctx == nullptr, "SSL_CTX_new failed");

                    static util::exitcall ec([]{
                        /* ::BIO_free(err_bio); */
                        ::SSL_CTX_free(ssl_ctx);
                        ::ERR_free_strings();
                    });
                    log_info("ssl library inited");
                    return true;
                }
                static bool load_certificate(const char* ca_cert_file,
                                             const char* cert_file,
                                             const char* key_file,
                                             bool verify_cert = false,
                                             bool load_private_key = true) {

                    if(verify_cert) {
                        // 要求核查对方证书
                        /* ::SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL); */
                    }
                    else {
                        // 不要求核查证书
                        ::SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
                    }
                    if(load_private_key) {
                        // 加载CA证书
                        if(!::SSL_CTX_load_verify_locations(ssl_ctx, ca_cert_file, nullptr)) {
                            log_error("SSL_CTX_load_verify_locations error");
                            ::ERR_print_errors_fp(stderr);
                            ::exit(1);
                        }
                        // 加载自己的证书
                        if(::SSL_CTX_use_certificate_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
                            log_error("SSL_CTX_use_certificate_file error");
                            ::ERR_print_errors_fp(stdout);
                            ::exit(1);
                        }
                        // 加载自己的私钥
                        if(::SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
                            log_error("SSL_CTX_use_PrivateKey_file error");
                            ::ERR_print_errors_fp(stdout);
                            ::exit(1);
                        }
                        // 判断私钥是否正确
                        if(!::SSL_CTX_check_private_key(ssl_ctx)) {
                            log_error("SSL_CTX_check_private_key error");
                            ::ERR_print_errors_fp(stdout);
                            ::exit(1);
                        }
                    }
                    return 0;
                }
                static ::SSL* new_ssl_and_set_fd(int fd) {
                    exitif(ssl_ctx == nullptr, "ssl_ctx is nullptr");
                    ::SSL* ssl = ::SSL_new(ssl_ctx);
                    exitif(ssl == nullptr, "SSL_new failed");
                    int r = ::SSL_set_fd(ssl, fd);
                    exitif(!r, "SSL_set_fd failed");
                    return ssl;
                }
                static void close(::SSL* ssl) {
                    ::SSL_shutdown(ssl);
                    ::SSL_free(ssl);
                }
                static bool connect(::SSL* ssl) {
                    int ret = ::SSL_connect(ssl);
                    log_debug(ret);
                    if(ret != 1) {
                        log_error("connec to server ssl error");
                        return false;
                    }
                    else {
                        return true;
                    }
                }
                static bool is_connecting(::SSL* ssl) {
                    int err = ::SSL_get_error(ssl, -1);
                    if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                        return true;
                    }
                    else {
                        return false;
                    }
                }
                static bool accept(::SSL* ssl) {
                    int ret = ::SSL_accept(ssl);
                    log_debug(ret);
                    return ret != 1 ? false : true;
                }
                static int readable(::SSL* ssl) {
                    return ::SSL_pending(ssl);
                }
                static int recv(::SSL* ssl, char* buffer, int bytes) {
                    int read_bytes = ::SSL_read(ssl, buffer, bytes);
                    int ssl_error = ::SSL_get_error(ssl, read_bytes);
                    if(read_bytes < 0 && (ssl_error == SSL_ERROR_WANT_READ)) {
                        log_error("SSL_read error", ssl_error, "errno", errno, "msg", std::strerror(errno));
                    }
                    return read_bytes;
                }
                static int send(::SSL* ssl, const char* buffer, int bytes) {
                    int write_bytes = ::SSL_write(ssl, buffer, bytes);
                    int ssl_error = ::SSL_get_error(ssl, write_bytes);
                    if(write_bytes < 0 && ssl_error == SSL_ERROR_WANT_WRITE) {
                        log_error("SSL_write error", ssl_error, "errno", errno, "msg", std::strerror(errno));
                    }
                    return write_bytes;
                }
                static bool handshake(::SSL* ssl) {
                    ::SSL_set_accept_state(ssl);
                    int ret = ::SSL_do_handshake(ssl);
                    if(ret == 1) {
                        return true;
                    }
                    else {
                        log_error("handshake error");
                        return false;
                    }
                }
            private:
                static ::SSL_CTX* ssl_ctx;
        };
        ::SSL_CTX* ssl::ssl_ctx = nullptr;
#endif
    }

    namespace udp
    {
        class sockets
        {
            public:
                static int block_socket(bool ip4 = true) {
                    (void)ip4;
                    return ::socket(AF_INET, SOCK_DGRAM, 0);
                }
                static int nonblock_socket(bool ip4 = true) {
                    int fd = block_socket(ip4);
                    return set_nonblock(fd);
                }
                static bool set_nonblock(int fd) {
                    return tcp::sockets::set_nonblock(fd);
                }
                static bool set_block(int fd) {
                    return tcp::sockets::set_block(fd);
                }
                static bool close(int fd) {
                    return tcp::sockets::close(fd);
                }
                static bool bind(int fd, std::string_view ip, unsigned short port) {
                    return tcp::sockets::bind(fd, ip, port);
                }
                static int readable(int fd) {
                    return tcp::sockets::readable(fd);
                }
                static int recv(int fd, char* buffer, int len, std::string& ip, unsigned short& port) {
                    struct sockaddr_in sockaddr;
                    socklen_t size = sizeof(sockaddr);
                    int bytes = ::recvfrom(fd, buffer, len, 0, (struct sockaddr*)&sockaddr, &size);
                    auto [ip_addr, p] = address::parse_ip_port(sockaddr);
                    ip = std::move(ip_addr);
                    port = std::move(p);
                    return bytes;
                }
                static int send(int fd, const char* buffer, int len, std::string_view ip, unsigned short port) {
                    struct sockaddr addr = address::to_sockaddr(ip, port);
                    int bytes = ::sendto(fd, buffer, len, 0, &addr, sizeof(addr));
                    return bytes;
                }
        };
    }
}
