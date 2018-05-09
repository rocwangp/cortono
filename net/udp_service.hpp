#pragma once
#include "../std.hpp"
#include "../ip/sockets.hpp"
#include "eventloop.hpp"
#include "udp_connection.hpp"

namespace cortono::net
{
    template <typename Connection>
    class UdpService
    {
        public:
            using read_callback_t = std::function<void(std::shared_ptr<Connection>)>;

            UdpService(EventLoop* loop, const std::string& ip, std::uint16_t port)
                : loop_(loop),
                  sockfd_(ip::udp::sockets::block_socket()),
                  conn_ptr_(std::make_shared<Connection>(loop, sockfd_, ip, port)),
                  poller_cb_(std::make_shared<EventPoller::PollerCB>())
            {
                if(ip::udp::sockets::bind(sockfd_, ip, port) == false) {
                    log_fatal("bind error", std::strerror(errno));
                }
                poller_cb_->read_cb = std::bind(&UdpService::handle_read, this);
                loop->poller()->update(sockfd_, EventPoller::NONE_EVENT, EventPoller::READ_EVENT, poller_cb_);
            }
            ~UdpService() {
                ip::udp::sockets::close(sockfd_);
            }
            auto conn_ptr() {
                return conn_ptr_;
            }
            void on_read(read_callback_t cb) {
                read_cb_ = std::move(cb);
            }
        private:
            void handle_read() {
                exitif(read_cb_ == nullptr, "read_cb is nullptr");
                read_cb_(conn_ptr_);

            void handle_write() {
                log_info("in handle_write");
            }
            void handle_close() {
                log_info("in handle_close");
            }
        private:
            EventLoop* loop_;
            int sockfd_;
            std::shared_ptr<Connection> conn_ptr_;
            std::shared_ptr<EventPoller::PollerCB> poller_cb_;

            read_callback_t read_cb_;
    };
}
