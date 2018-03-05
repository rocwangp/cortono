#pragma once

#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <queue>
#include <future>
#include <functional>
#include <algorithm>
#include <string_view>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cerrno>

#include "buffer.h"
#include "poller.h"
#include "../ip/sockets.h"
#include "../util/util.h"
#include "../util/noncopyable.h"

namespace cortono::net
{
    class cort_socket : private util::noncopyable
    {
        public:
            enum socket_option
            {
                BLOCK,
                NON_BLOCK,
                REUSE_ADDR,
                REUSE_POST,
                NO_DELAY
            };

            enum socket_state
            {
                INIT,
                ADDED,
                DELETED
            };
        public:

            cort_socket()
                : cort_socket(ip::tcp::sockets::block_socket())
            {

            }

            cort_socket(int fd)
                : fd_(fd),
                  events_(cort_poller::NONE_EVENT),
                  poller_cbs_(std::make_shared<cort_poller::CB>()),
                  read_buffer_(std::make_shared<cort_buffer>()),
                  write_buffer_(std::make_shared<cort_buffer>())
            {

            }

            ~cort_socket() {
                ip::tcp::sockets::close(fd_);
            }

            bool bind(std::string_view ip, unsigned short port) {
                return ip::tcp::sockets::bind(fd_, ip, port);
            }

            bool listen(long long int listen_nums = INT64_MAX) {
                return ip::tcp::sockets::listen(fd_, listen_nums);
            }

            int accept() {
                return ip::tcp::sockets::accept(fd_);
            }

            bool close() {
                assert(poller_cbs_->close_cb);
                poller_cbs_->close_cb();
                return true;
            }

            bool connect(std::string_view ip, unsigned short port) {
                return ip::tcp::sockets::connect(fd_, ip, port);
            }

            void tie(std::shared_ptr<cort_poller> poller) {
                weak_poller_ = poller;
            }

            void enable_read(std::function<void()> cb) {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller_cbs_->read_cb = std::move(cb);
                    poller->update(fd_, events_, events_ | cort_poller::READ_EVENT, poller_cbs_);
                    events_ |= cort_poller::READ_EVENT;
                }
                else {
                    log_fatal("poller is not exist");
                }
            }

            void enable_write(std::function<void()> cb) {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller_cbs_->write_cb = std::move(cb);
                    poller->update(fd_, events_, events_ | cort_poller::WRITE_EVENT, poller_cbs_);
                    events_ |= cort_poller::WRITE_EVENT;
                }
                else {
                    log_fatal("poller is not exist");
                }
            }

            void enable_close(std::function<void()> cb) {
                poller_cbs_->close_cb = std::move(cb);
            }

            void disable_write() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, events_ & (~cort_poller::WRITE_EVENT), poller_cbs_);
                    events_ &= (~cort_poller::WRITE_EVENT);
                }
                else {
                    log_fatal("poller is not exist");
                }
            }

            void disable_all() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, cort_poller::NONE_EVENT, poller_cbs_);
                    events_ = cort_poller::NONE_EVENT;
                }
                else {
                    log_fatal("poller is not exist");
                }
            }

            std::string local_address() const {
                return ip::address::local_address(fd_);
            }

            std::string peer_address() const {
                return ip::address::peer_address(fd_);
            }

            bool recv_to_buffer() {
                if(int bytes = ip::tcp::sockets::readable(fd_); bytes > 0) {
                    std::cout << read_buffer_.get() << std::endl;
                    read_buffer_->enable_bytes(bytes);
                    std::cout << read_buffer_.get() << std::endl;
                    if(int read_bytes = ip::tcp::sockets::recv(fd_, read_buffer_->end(), bytes); read_bytes > 0) {
                        read_buffer_->append_bytes(read_bytes);
                        return true;
                    }
                }
                log_info("read error, close connection");
                poller_cbs_->close_cb();
                return false;
            }

            std::string read_all() {
                return read_buffer_->read_all();
            }

            std::string read_util(std::string_view s) {
                return read_buffer_->read_util(s);
            }

            bool write_done() const {
                return write_buffer_->empty();
            }

            cort_socket& write(const std::string& msg) {
                int write_bytes = 0;
                if(!write_buffer_->empty() ||
                   ((write_bytes = ip::tcp::sockets::send(fd_, msg)) < static_cast<int>(msg.size())))
                {
                    write_bytes = std::max(write_bytes, 0);
                    write_buffer_->append(msg.substr(write_bytes));
                    enable_write([this] {
                        int write_bytes = ip::tcp::sockets::send(
                                fd_, write_buffer_->begin(), write_buffer_->readable());
                        if(write_bytes == write_buffer_->readable()){
                            disable_write();
                            do_write_callback();
                        }
                        else {
                            write_buffer_->append_bytes(std::max(0, write_bytes));
                        }
                    });
                }
                return *this;
            }

            void do_write_callback() {
                while(!write_cbs_.empty()) {
                    auto task = write_cbs_.front();
                    write_cbs_.pop();
                    task();
                }
            }
            template <typename Function, typename... Args>
            cort_socket& then(Function&& f, Args... args) {
                if(write_done()) {
                    f(args...);
                    return *this;
                }
                using result_type = typename std::invoke_result_t<Function, Args...>;
                auto task = std::make_shared<std::packaged_task<result_type()>>(
                    std::bind(std::forward<Function>(f), std::forward<Args>(args)...));
                write_cbs_.emplace([task]{
                    (*task)();
                });
                return *this;
            }


            void enable_option() { }

            template <class... Args>
            void enable_option(socket_option opt, Args... args) {
                util::exitif(!opt_functors_[opt](fd_), static_cast<int>(opt), std::strerror(errno));
                enable_option(args...);
            }

            int fd() {
                return fd_;
            }

            auto read_buffer() {
                return read_buffer_;
            }
        private:
            int fd_;
            uint32_t events_;
            std::weak_ptr<cort_poller> weak_poller_;
            std::shared_ptr<cort_poller::CB> poller_cbs_;
            std::shared_ptr<cort_buffer> read_buffer_;
            std::shared_ptr<cort_buffer> write_buffer_;
            std::queue<std::function<void()>> write_cbs_;

            static std::map<socket_option, std::function<bool(int)>> opt_functors_;
    };

    std::map<cort_socket::socket_option, std::function<bool(int)>> cort_socket::opt_functors_ = {
        { cort_socket::BLOCK,      [](int fd) { return ip::tcp::sockets::set_block(fd); }},
        { cort_socket::NON_BLOCK,  [](int fd) { return ip::tcp::sockets::set_nonblock(fd); }},
        { cort_socket::REUSE_ADDR, [](int fd) { return ip::tcp::sockets::reuse_address(fd); }},
        { cort_socket::REUSE_POST, [](int fd) { return ip::tcp::sockets::reuse_post(fd); }},
        { cort_socket::NO_DELAY,   [](int fd) { return ip::tcp::sockets::no_delay(fd); }}
    };

}
