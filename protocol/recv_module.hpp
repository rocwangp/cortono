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

            // return false if the data seq in invalid range(controlled by send_window)
            template <typename Packet>
            bool check(Packet& packet) {
                if(packet.is_error_packet()) {

                }
                else if(packet.is_recv_data_packet()) {
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
                    // move recv window and store data
                    // modify ack
                    // because the main module need to send ack packet for this data packet
                    // store src ip and src port, provide to app if needed

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
