#pragma once

#include "buffer.hpp"
#include "poller.hpp"
#include "../std.hpp"
#include "../ip/sockets.hpp"
#include "../util/util.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class TcpSocket
    {
        public:
            enum socket_option
            {
                block,
                non_block,
                reuse_addr,
                reuse_port,
                no_delay
            };

            typedef std::shared_ptr<TcpSocket> Pointer;
            typedef std::function<void()>      EventCallBack;
        public:

            TcpSocket() : TcpSocket(ip::tcp::sockets::block_socket())
            { }

            TcpSocket(int fd)
                : fd_(fd),
                  events_(EventPoller::NONE_EVENT),
                  poller_cbs_(std::make_shared<EventPoller::PollerCB>())
            { }

            ~TcpSocket() { ip::tcp::sockets::close(fd_); }

            bool bind(std::string_view ip, unsigned short port) {
                return ip::tcp::sockets::bind(fd_, ip, port);
            }

            bool listen(long long int listen_nums = std::numeric_limits<int>::max()) {
                return ip::tcp::sockets::listen(fd_, listen_nums);
            }
            int accept() {
                return ip::tcp::sockets::accept(fd_);
            }
            bool close() {
                exitif(poller_cbs_->close_cb != nullptr, "close cb is nullptr");
                poller_cbs_->close_cb();
                return true;
            }
            bool connect(std::string_view ip, unsigned short port) {
                return ip::tcp::sockets::connect(fd_, ip, port);
            }
            // 非阻塞connect的后续处理，当fd可读时会调用，进而判断是否可写
            bool handshake() {
                struct pollfd pfd;
                pfd.fd = fd_;
                pfd.events = POLLOUT | POLLERR;
                int error = 0;
                if(::poll(&pfd, 1, 0) == 1 && (error = ip::tcp::sockets::get_error(fd_)) == 0) {
                    return true;
                }
                else {
                    log_info(std::strerror(error));
                    return false;
                }
            }
            void tie(std::shared_ptr<EventPoller> poller) {
                weak_poller_ = poller;
            }
            void enable_reading() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, events_ | EventPoller::READ_EVENT, poller_cbs_);
                    events_ |= EventPoller::READ_EVENT;
                }
                else {
                    log_fatal("poller is not exist");
                }
            }
            void enable_writing() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, events_ | EventPoller::WRITE_EVENT, poller_cbs_);
                    events_ |= EventPoller::WRITE_EVENT;
                }
                else {
                    log_fatal("poller is not exist");
                }
            }
            void disable_reading() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, events_ & (~EventPoller::READ_EVENT), poller_cbs_);
                    events_ &= (~EventPoller::READ_EVENT);
                }
                else {
                    log_fatal("poller is not exist");
                }
            }
            void disable_writing() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, events_ & (~EventPoller::WRITE_EVENT), poller_cbs_);
                    events_ &= (~EventPoller::WRITE_EVENT);
                }
                else {
                    log_fatal("poller is not exist");
                }
            }
            void disable_all() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, EventPoller::NONE_EVENT, poller_cbs_);
                    events_ = EventPoller::NONE_EVENT;
                }
                else {
                    log_fatal("poller is not exist");
                }
            }
            std::pair<std::string, std::uint16_t> local_endpoint() const {
                return ip::address::local_endpoint(fd_);
            }
            std::pair<std::string, std::uint16_t> peer_endpoint() const {
                return ip::address::peer_endpoint(fd_);
            }
            std::string local_address() const {
                return ip::address::local_address(fd_);
            }
            std::string peer_address() const {
                return ip::address::peer_address(fd_);
            }
            std::string peer_ip() const {
                return ip::address::peer_endpoint(fd_).first;
            }
            std::uint16_t peer_port() const {
                return ip::address::peer_endpoint(fd_).second;
            }
            std::string local_ip() const {
                return ip::address::local_endpoint(fd_).first;
            }
            std::uint16_t local_port() const {
                return ip::address::local_endpoint(fd_).second;
            }

            template <class... Args>
            void set_option(socket_option opt, Args... args) {
                if(!opt_functors_[opt](fd_)) {
                    log_error("fail to set option", static_cast<int>(opt), fd_);
                }
                if constexpr (sizeof...(Args) > 0) {
                    set_option(args...);
                }
            }

            int fd() const {
                return fd_;
            }
            void set_read_callback(EventCallBack cb) {
                poller_cbs_->read_cb = cb;
            }
            void set_write_callback(EventCallBack cb) {
                poller_cbs_->write_cb = cb;
            }
            void set_close_callback(EventCallBack cb) {
                poller_cbs_->close_cb = cb;
            }
            int send(const char* buffer, int len) {
                return ip::tcp::sockets::send(fd_, buffer, len);
            }
            int recv(char* buffer, int len) {
                return ip::tcp::sockets::recv(fd_, buffer, len);
            }
            int readable() {
                return ip::tcp::sockets::readable(fd_);
            }
            int sendfile(const std::string& filename, off_t fileoffet, std::size_t filesize) {
                return ip::tcp::sockets::sendfile(fd_, filename, fileoffet, filesize);
            }
        protected:
            int fd_;
            uint32_t events_;
            std::weak_ptr<EventPoller> weak_poller_;
            std::shared_ptr<EventPoller::PollerCB> poller_cbs_;
            static std::map<socket_option, std::function<bool(int)>> opt_functors_;
    };

    inline std::map<TcpSocket::socket_option, std::function<bool(int)>> TcpSocket::opt_functors_ = {
        { TcpSocket::block,      [](int fd) { return ip::tcp::sockets::set_block(fd); }},
        { TcpSocket::non_block,  [](int fd) { return ip::tcp::sockets::set_nonblock(fd); }},
        { TcpSocket::reuse_addr, [](int fd) { return ip::tcp::sockets::reuse_address(fd); }},
        { TcpSocket::reuse_port, [](int fd) { return ip::tcp::sockets::reuse_post(fd); }},
        { TcpSocket::no_delay,   [](int fd) { return ip::tcp::sockets::no_delay(fd); }}
    };

}
