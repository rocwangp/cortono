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
        private:
            void handle_read() {
                conn_ptr_->handle_read();
            }
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

    };
}
