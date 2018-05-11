#pragma once

#include "sr_header.hpp"
#include "slide_window.hpp"

namespace cortono
{

    template <std::uint64_t BufferSize, std::uint64_t WindowSize>
    class SendModule
    {
        public:
            SendModule(cortono::net::EventLoop* loop, const std::string& ip, const std::uint16_t& port)
                : loop_(loop),
                  ip_(ip),
                  port_(port)
            {  }

            SendModule(const SendModule& other)
                : loop_(other.loop_),
                  ip_(other.ip_),
                  port_(other.port_)
            {  }

            template <typename Packet>
            bool check(Packet& packet) {
                if(packet.is_error_packet()) {

                }
                else if(packet.is_recv_data_packet()) {

                }
                else if(packet.is_recv_ack_packet()) {

                }
                else if(packet.is_send_data_packet()) {
                    if(!send_window_.in_valid_range(seq_, seq_ + packet.data_size())) {
                        log_info("invalid range:", seq_, (seq_ + packet.data_size()) % BufferSize);
                        log_info("send window range:", send_window_.left_bound(), "to", send_window_.right_bound());
                        return false;
                    }
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
                    // recv_module has modified ack
                    // swap src_port and des_port to send ack packet
                    packet.set_des_ip(packet.src_ip());
                    packet.set_des_port(packet.src_port());
                    packet.set_src_ip(ip_);
                    packet.set_src_port(port_);
                    packet.set_ack_flag();
                    packet.clear_data();
                }
                else if(packet.is_recv_ack_packet()) {
                    // recv ack packet
                    // don't modify anything
                    // because resend_module will use the ack to cancel timer
                    // move send window if the ack is in valid range
                    send_window_.move_if_valid(packet.ack());
                }
                else if(packet.is_send_data_packet()) {
                    /* packet.set_src_port(port_); */
                    packet.set_seq(seq_);
                    if(BufferSize - seq_ > packet.data_size()) {
                        seq_ += packet.data_size();
                    }
                    else {
                        seq_ = packet.data_size() - (BufferSize - seq_);
                    }
                }
                else {

                }
                return true;
            }

        private:
            SlideWindow<BufferSize, WindowSize> send_window_;

            cortono::net::EventLoop* loop_;
            std::uint64_t seq_;
            std::string ip_;
            std::uint16_t port_;
    };
}
