#pragma once

#include "buffer.hpp"
#include "poller.hpp"
#include "../std.hpp"
#include "../ip/sockets.hpp"
#include "../util/util.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class tcp_socket : public std::enable_shared_from_this<tcp_socket>,
                        private util::noncopyable
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

        public:

            tcp_socket()
                : tcp_socket(ip::tcp::sockets::block_socket())
            {

            }

            tcp_socket(int fd)
                : fd_(fd),
                  events_(event_poller::none_event),
                  poller_cbs_(std::make_shared<event_poller::event_cb>()),
                  read_buffer_(std::make_shared<event_buffer>()),
                  write_buffer_(std::make_shared<event_buffer>())
            {

            }

            ~tcp_socket() {
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

            bool bind_and_listen(std::string_view ip, unsigned short port, long long listen_nums = INT64_MAX) {
                enable_option(reuse_addr, reuse_port, non_block);
                util::exitif(!bind(ip, port), "fail to bind <", ip, port, ">", std::strerror(errno));
                util::exitif(!listen(listen_nums), "fail to listen");
                return true;
            }

            void tie(std::shared_ptr<event_poller> poller) {
                weak_poller_ = poller;
            }

            void enable_read(std::function<void()> cb) {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller_cbs_->read_cb = std::move(cb);
                    poller->update(fd_, events_, events_ | event_poller::read_event, poller_cbs_);
                    events_ |= event_poller::read_event;
                }
                else {
                    log_fatal("poller is not exist");
                }
            }

            void enable_write(std::function<void()> cb) {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller_cbs_->write_cb = std::move(cb);
                    poller->update(fd_, events_, events_ | event_poller::write_event, poller_cbs_);
                    events_ |= event_poller::write_event;
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
                    poller->update(fd_, events_, events_ & (~event_poller::write_event), poller_cbs_);
                    events_ &= (~event_poller::write_event);
                }
                else {
                    log_fatal("poller is not exist");
                }
            }

            void disable_all() {
                if(auto poller = weak_poller_.lock(); poller) {
                    poller->update(fd_, events_, event_poller::NONE_EVENT, poller_cbs_);
                    events_ = event_poller::NONE_EVENT;
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
                    read_buffer_->enable_bytes(bytes);
                    if(int read_bytes = ip::tcp::sockets::recv(fd_, read_buffer_->end(), bytes); read_bytes > 0) {
                        read_buffer_->retrieve_write_bytes(read_bytes);
                        return true;
                    }
                }
                log_info("read 0 bytes or error, close connection");
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

            int write_file(std::string_view filename, int count) {
                if(filename.empty()) {
                    return -1;
                }
                else {
                    return ip::tcp::sockets::send_file(fd_, filename, count);
                }
            }

            auto write(const std::string& msg) {
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
                            write_buffer_->retrieve_read_bytes(std::max(0, write_bytes));
                        }
                    });
                }
                return shared_from_this();
            }


            void do_write_callback() {
                while(!write_cbs_.empty()) {
                    auto task = write_cbs_.front();
                    write_cbs_.pop();
                    task();
                }
            }

            template <typename Function, typename... Args>
            auto invoke_after_write_done(Function&& f, Args... args) {
                using result_type = typename std::invoke_result_t<Function, Args...>;
                auto task = std::make_shared<std::packaged_task<result_type()>>(
                    std::bind(std::forward<Function>(f), std::forward<Args>(args)...));
                write_cbs_.emplace([task]{
                    (*task)();
                });
                if(write_done()) {
                    do_write_callback();
                }
                return shared_from_this();
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
            std::weak_ptr<event_poller> weak_poller_;
            std::shared_ptr<event_poller::event_cb> poller_cbs_;
            std::shared_ptr<event_buffer> read_buffer_;
            std::shared_ptr<event_buffer> write_buffer_;
            std::queue<std::function<void()>> write_cbs_;

            static std::map<socket_option, std::function<bool(int)>> opt_functors_;
    };

    std::map<tcp_socket::socket_option, std::function<bool(int)>> tcp_socket::opt_functors_ = {
        { tcp_socket::block,      [](int fd) { return ip::tcp::sockets::set_block(fd); }},
        { tcp_socket::non_block,  [](int fd) { return ip::tcp::sockets::set_nonblock(fd); }},
        { tcp_socket::reuse_addr, [](int fd) { return ip::tcp::sockets::reuse_address(fd); }},
        { tcp_socket::reuse_port, [](int fd) { return ip::tcp::sockets::reuse_post(fd); }},
        { tcp_socket::no_delay,   [](int fd) { return ip::tcp::sockets::no_delay(fd); }}
    };

}
