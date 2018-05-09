#pragma once

#include "slide_window.hpp"
#include "parser_module.hpp"

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

            template <typename Parser>
            bool check(Parser& parser) {
                if(parser.is_recv_data_packet()) {

                }
                else if(parser.is_recv_ack_packet()) {

                }
                else if(parser.is_send_data_packet()) {
                    if(seq_ + parser.data_size() >= send_window_.right_bound()) {
                        return false;
                    }
                }
                else {

                }
                return true;
            }
            template <typename Parser>
            bool handle(Parser& parser) {
                if(parser.is_recv_data_packet()) {
                    // recv data packet
                    // recv_module has modified ack
                    // swap src_port and des_port
                    parser.swap_port();
                    parser.set_ack_flag();
                    parser.clear_data();
                }
                else if(parser.is_recv_ack_packet()) {
                    // recv ack packet
                    // don't modify anything
                    // because resend_module will use the ack to cancel timer
                    // move send window if the ack is in valid range
                    send_window_.move_if_valid(parser.ack());
                }
                else if(parser.is_send_data_packet()) {
                    parser.set_src_port(port_);
                    parser.set_seq(seq_);
                    seq_ += parser.data_size();
                }
                else {

                }
                return true;
            }

        private:
            void modify_parser() {
            }
            template <typename Parser>
            void send_packet(Parser& parser) {
                parser.set_src_port = port_;
                parser.set_seq(seq_);
            }
        private:
            SlideWindow<BufferSize, WindowSize> send_window_;

            cortono::net::EventLoop* loop_;
            std::uint64_t seq_;
            std::string ip_;
            std::uint16_t port_;
    };
}
