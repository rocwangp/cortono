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
                std::string ip_addr { ip.data(), ip.length() };
                ::inet_pton(AF_INET, ip_addr.data(), &addr.sin_addr);
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
                    return ::accept(sockfd, nullptr, nullptr);
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
                static int sendfile(int fd, const std::string& filename, off_t offet, std::size_t count) {
                    int in_fd = util::io::open(filename);
                    if(in_fd == -1) {
                        log_error(strerror(errno));
                    }
                    int n = ::sendfile(fd, in_fd, &offet, count);
                    if(n == -1) {
                        log_error(std::strerror(errno));
                    }
                    util::io::close(in_fd);
                    return n;
                }
            private:
                static int idle_fd;
        };

#ifdef CORTONO_USE_SSL
#define CA_CERT_FILE "../http/ssl/ca.crt"
#define SERVER_CERT_FILE "../http/ssl/server.crt"
#define SERVER_KEY_FILE "../http/ssl/server.key"

        class ssl
        {
            public:
                static bool init_ssl() {
                    ::SSL_load_error_strings();
                    int r = ::SSL_library_init();
                    exitif(!r, "SSL_library_init failed");
                    ::OPENSSL_add_all_algorithms_conf();
                    ssl_ctx = ::SSL_CTX_new(::SSLv23_server_method());
                    exitif(ssl_ctx == nullptr, "SSL_CTX_new failed");

                    if(!::SSL_CTX_load_verify_locations(ssl_ctx, CA_CERT_FILE, nullptr)) {
                        ::ERR_print_errors_fp(stderr);
                        ::exit(1);
                    }
                    if(::SSL_CTX_use_certificate_file(ssl_ctx, SERVER_CERT_FILE, SSL_FILETYPE_PEM) <= 0) {
                        ::ERR_print_errors_fp(stdout);
                        ::exit(1);
                    }
                    if(::SSL_CTX_use_PrivateKey_file(ssl_ctx, SERVER_KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
                        ::ERR_print_errors_fp(stdout);
                        ::exit(1);
                    }
                    if(::SSL_CTX_check_private_key(ssl_ctx) <= 0) {
                        ::ERR_print_errors_fp(stdout);
                        ::exit(1);
                    }
                    static util::exitcall ec([]{
                        ::BIO_free(err_bio);
                        ::SSL_CTX_free(ssl_ctx);
                        ::ERR_free_strings();
                    });
                    log_info("ssl library inited");
                    return 0;
                }
                static ::SSL* new_ssl_and_set_fd(int fd) {
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
                    return ::SSL_connect(ssl) == -1 ? false : true;
                }
                static bool accept(::SSL* ssl) {
                    return ::SSL_accept(ssl) == -1 ? false : true;
                }
                static int readable(::SSL* ssl) {
                    return ::SSL_pending(ssl);
                }
                static int recv(::SSL* ssl, char* buffer, int bytes) {
                    int read_bytes = ::SSL_read(ssl, buffer, bytes);
                    int ssl_error = ::SSL_get_error(ssl, read_bytes);
                    if(read_bytes < 0 && ssl_error == SSL_ERROR_WANT_READ) {
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
            private:
                static ::BIO* err_bio;
                static ::SSL_CTX* ssl_ctx;
        };
        ::BIO* ssl::err_bio = nullptr;
        ::SSL_CTX* ssl::ssl_ctx = nullptr;
#endif
    }
}
