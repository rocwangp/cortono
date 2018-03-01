#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>

#include <cstdint>
#include <cstring>
#include <cerrno>

#include "buffer.h"
#include "poller.h"
#include "../ip/sockets.h"
#include "../util/util.h"

namespace cortono
{
    namespace net
    {

        class Socket
        {
            public:
                enum SocketOpt
                {
                    BLOCK,
                    NON_BLOCK,
                    REUSE_ADDR,
                    REUSE_POST
                };

                enum State
                {
                    INIT,
                    ADDED,
                    DELETED
                };
            public:

                Socket()
                    : Socket(ip::tcp::sockets::block_socket())
                {

                }

                Socket(int fd)
                    : fd_(fd),
                      events_(Poller::NONE_EVENT),
                      poller_cbs_(std::make_shared<Poller::CB>()),
                      read_buffer_(std::make_shared<Buffer>()),
                      write_buffer_(std::make_shared<Buffer>())
                {

                }

                ~Socket()
                {
                    ip::tcp::sockets::close(fd_);
                }

                bool bind(const std::string& ip, unsigned short port)
                {
                    return ip::tcp::sockets::bind(fd_, ip, port);
                }

                bool listen(long long int listen_nums = INT64_MAX)
                {
                    return ip::tcp::sockets::listen(fd_, listen_nums);
                }

                int accept()
                {
                    return ip::tcp::sockets::accept(fd_);
                }

                bool close()
                {
                    return ip::tcp::sockets::close(fd_);
                }

                bool connect(const std::string& ip, unsigned short port)
                {
                    return ip::tcp::sockets::connect(fd_, ip, port);
                }

                void tie(std::shared_ptr<Poller> poller)
                {
                    weak_poller_ = poller;
                }

                void enable_read(std::function<void()> cb)
                {
                    if(auto poller = weak_poller_.lock(); poller)
                    {
                        poller_cbs_->read_cb = std::move(cb);
                        poller->update(fd_, events_, events_ | Poller::READ_EVENT, poller_cbs_);
                        events_ |= Poller::READ_EVENT;
                    }
                    else
                    {
                        log_fatal("poller is not exist");
                    }
                }

                void enable_write(std::function<void()> cb)
                {
                    if(auto poller = weak_poller_.lock(); poller)
                    {
                        poller_cbs_->write_cb = std::move(cb);
                        poller->update(fd_, events_, events_ | Poller::WRITE_EVENT, poller_cbs_);
                        events_ |= Poller::WRITE_EVENT;
                    }
                    else
                    {
                        log_fatal("poller is not exist");
                    }
                }

                void enable_close(std::function<void()> cb)
                {
                    poller_cbs_->close_cb = std::move(cb);
                }

                void disable_write()
                {
                    if(auto poller = weak_poller_.lock(); poller)
                    {
                        poller->update(fd_, events_, events_ & (~Poller::WRITE_EVENT), poller_cbs_);
                        events_ &= (~Poller::WRITE_EVENT);
                    }
                    else
                    {
                        log_fatal("poller is not exist");
                    }
                }

                void disable_all()
                {
                    if(auto poller = weak_poller_.lock(); poller)
                    {
                        poller->update(fd_, events_, Poller::NONE_EVENT, poller_cbs_);
                        events_ = Poller::NONE_EVENT;
                    }
                    else
                    {
                        log_fatal("poller is not exist");
                    }
                }

                std::string local_address() const
                {
                    return ip::address::local_address(fd_);
                }

                std::string peer_address() const
                {
                    return ip::address::peer_address(fd_);
                }

                void recv_to_buffer()
                {
                    if(int bytes = ip::tcp::sockets::readable(fd_); bytes > 0)
                    {
                        read_buffer_->enable_bytes(bytes);
                        if(int read_bytes = ip::tcp::sockets::recv(fd_, read_buffer_->end(), bytes); read_bytes > 0)
                        {
                            read_buffer_->append_bytes(read_bytes);
                            return;
                        }
                    }
                    poller_cbs_->close_cb();
                }

                std::string recv_all()
                {
                    return read_buffer_->read_all();
                }

                void send(const std::string& msg)
                {
                    int write_bytes = 0;
                    if(!write_buffer_->empty() ||
                       ((write_bytes = ip::tcp::sockets::send(fd_, msg)) < static_cast<int>(msg.size())))
                    {
                        write_bytes = std::max(write_bytes, 0);
                        write_buffer_->append(msg.substr(write_bytes));
                        enable_write(
                                [this]
                                {
                                    int write_bytes = ip::tcp::sockets::send(
                                            fd_, write_buffer_->begin(), write_buffer_->readable());
                                    if(write_bytes == write_buffer_->readable())
                                        disable_write();
                                    else
                                        write_buffer_->append_bytes(std::max(0, write_bytes));
                                }
                            );
                    }
                }


                void enable_option() { }

                template <class... Args>
                void enable_option(SocketOpt opt, Args... args)
                {
                    util::exitif(!opt_functors_[opt](fd_), static_cast<int>(opt), std::strerror(errno));
                    enable_option(args...);
                }

                int fd() { return fd_; }
            private:
                int fd_;
                uint32_t events_;
                std::weak_ptr<Poller> weak_poller_;
                std::shared_ptr<Poller::CB> poller_cbs_;
                std::shared_ptr<Buffer> read_buffer_;
                std::shared_ptr<Buffer> write_buffer_;

                static std::map<SocketOpt, std::function<bool(int)>> opt_functors_;
        };

        std::map<Socket::SocketOpt, std::function<bool(int)>> Socket::opt_functors_ =
        {
            { Socket::BLOCK,      [](int fd) { return ip::tcp::sockets::set_block(fd); }},
            { Socket::NON_BLOCK,  [](int fd) { return ip::tcp::sockets::set_nonblock(fd); }},
            { Socket::REUSE_ADDR, [](int fd) { return ip::tcp::sockets::reuse_address(fd); }},
            { Socket::REUSE_POST, [](int fd) { return ip::tcp::sockets::reuse_post(fd); }}
        };

    }
}
