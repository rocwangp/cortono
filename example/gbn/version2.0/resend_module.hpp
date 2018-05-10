#pragma once

#include "../../../cortono.hpp"

namespace cortono
{

    template <std::uint64_t BufferSize, std::uint64_t Time>
    class ResendModule
    {
        public:
            typedef std::function<void(const std::string&, const std::string&, std::uint16_t)> Sender;

            ResendModule(cortono::net::EventLoop* loop, const std::string& ip, const std::uint16_t& port)
                : loop_(loop),
                  ip_(ip),
                  port_(port)
            {
            }
            ~ResendModule() {
            }

            ResendModule(ResendModule&& other)
                : sender_(std::move(other.sender_)),
                  loop_(std::move(other.loop_)),
                  ip_(std::move(other.ip_)),
                  port_(std::move(other.port_)),
                  timers_(std::move(other.timers_))
            {  }

            ResendModule(const ResendModule& other)
                : sender_(other.sender_),
                  loop_(other.loop_),
                  ip_(other.ip_),
                  port_(other.port_),
                  timers_(other.timers_)
            {
            }

            template <typename Parser>
            bool check(Parser& parser) {
                if(parser.is_error_packet()) {

                }
                else if(parser.is_recv_data_packet()) {

                }
                else if(parser.is_recv_ack_packet()) {

                }
                else if(parser.is_send_data_packet()) {

                }
                else {

                }
                return true;
            }
            template <typename Parser>
            bool handle(Parser& parser) {
                if(parser.is_error_packet()) {
                    log_info("error packet");

                }
                else if(parser.is_recv_data_packet()) {
                    // recv data packet
                    // don't do anything
                }
                else if(parser.is_recv_ack_packet()) {
                    // recv ack packet
                    // cancel timer according to ack
                    cancel_timer(parser);
                }
                else if(parser.is_send_data_packet()) {
                    set_timer(parser);
                }
                else {

                }
                return true;
            }

            void bind_sender(Sender sender) {
                sender_ = std::move(sender);
            }

        private:
            template <typename Parser>
            void cancel_timer(Parser& parser) {
                // 假定recv_module和send_module没有修改ack
                auto ack = parser.ack();
                while(!timers_.empty()) {
                    auto start_seq = std::get<0>(timers_.front());
                    auto end_seq = std::get<1>(timers_.front());
                    // 假定ack在发送窗口的有效范围内
                    if(start_seq == ack) {
                        break;
                    }
                    log_info("cancel timer:", start_seq, "to", end_seq);
                    loop_->cancel_timer(std::get<2>(timers_.front()));
                    timers_.pop_front();
                }
            }
            template <typename Parser>
            void set_timer(Parser& parser) {
                // 假定send_module已经将parser中的seq和data更改
                std::uint64_t start_seq = parser.seq();
                std::uint64_t end_seq = (start_seq + parser.data_size()) % BufferSize;
                std::string packet_str = parser.to_string();
                auto des_port = parser.des_port();
                auto id = loop_->run_every(std::chrono::milliseconds(Time),
                    [=, packet_str = std::move(packet_str)]{
                    log_info("resend packet:", start_seq, "to", end_seq);
                    sender_(packet_str, ip_, des_port);
                });
                timers_.emplace_back(start_seq, end_seq, id);
            }
        private:
            Sender sender_;
            cortono::net::EventLoop* loop_{ nullptr };
            std::list<std::tuple<std::uint64_t, std::uint64_t, cortono::net::Timer::timer_id>> timers_;

            std::string ip_;
            std::uint16_t port_;
    };
}
