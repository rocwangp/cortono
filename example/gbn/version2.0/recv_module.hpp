#pragma once

#include "sr_header.hpp"
#include "slide_window.hpp"

namespace cortono
{

    template <std::uint64_t BufferSize, std::uint64_t WindowSize>
    class RecvModule
    {
        public:
            RecvModule(cortono::net::EventLoop* loop, const std::string& ip, const std::uint16_t& port)
                : loop_(loop),
                  ip_(ip),
                  port_(port)
            {  }

            RecvModule(const RecvModule& other)
                : recv_window_(other.recv_window_),
                  loop_(other.loop_),
                  ip_(other.ip_),
                  port_(other.port_)
            {  }

            RecvModule& operator=(const RecvModule& other) {
                if(this == &other) {
                    return *this;
                }
                else {
                    recv_window_ = other.recv_window_;
                    loop_ = other.loop_;
                    ip_ = other.ip_;
                    port_ = other.port_;
                    return *this;
                }
            }
            RecvModule(RecvModule&& other)
                : recv_window_(std::move(other.recv_window_)),
                  loop_(std::move(other.loop_)),
                  ip_(std::move(other.ip_)),
                  port_(std::move(other.port_))
            {  }

            RecvModule&& operator=(RecvModule&& other) {
                RecvModule tmp(std::move(other));
                std::swap(tmp, *this);
                return *this;
            }

            template <typename Packet>
            bool check(Packet& packet) {
                if(packet.is_error_packet()) {

                }
                else if(packet.is_recv_data_packet()) {
                    if(!recv_window_.in_valid_range(packet.seq(), packet.seq() + packet.data_size()) || recv_window_.full()) {
                        return false;
                    }
                }
                else if(packet.is_recv_ack_packet()) {

                }
                else if(packet.is_send_data_packet()) {

                }
                else {

                }
                return true;
            }
            template <typename Packet>
            bool handle(Packet& packet) {
                if(packet.is_error_packet()) {
                    log_info("error packet");
                }
                else if(packet.is_recv_data_packet()) {
                    // recv data packet
                    // move recv window if the seq is in valid range([win_left_, win_right_))
                    // modify ack
                    // because main module need to send ack packet for this data packet
                    peer_ip_ = packet.src_ip();
                    peer_port_ = packet.src_port();

                    auto start_seq = packet.seq();
                    auto data_size = packet.data_size();
                    recv_window_.set_flag_if_valid(packet.data(), start_seq, data_size);

                    packet.set_seq(0);
                    packet.set_ack(recv_window_.left_bound());
                }
                else if(packet.is_recv_ack_packet()){
                    // recv ack packet
                    // don't modify anything
                    // because resend_modudle need to use ack to cancel timer
                }
                else if(packet.is_send_data_packet()) {

                }
                else {

                }
                return true;
            }

            std::string recv_all() {
                return recv_window_.recv_all();
            }
            auto peer_ip_port() const {
                return std::make_pair(peer_ip_, peer_port_);
            }
        private:
            SlideWindow<BufferSize, WindowSize> recv_window_;

            cortono::net::EventLoop* loop_;
            std::string ip_;
            std::uint16_t port_;

            std::string peer_ip_;
            std::uint16_t peer_port_;
    };
}
