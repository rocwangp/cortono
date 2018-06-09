#pragma once

#include "sr_header.hpp"

namespace cortono
{
    class ModifyModule
    {
        public:
            ModifyModule(cortono::net::EventLoop* loop, const std::string& ip, std::uint16_t port)
                : loop_(loop),
                  local_ip_(ip),
                  local_port_(port)
            {  }

            template <typename Packet>
            bool check(Packet&) {
                return true;
            }

            template <typename Packet>
            bool handle(Packet& packet) {
                if(packet.is_error_packet()) {

                }
                else if(packet.is_recv_data_packet()) {
                    packet.set_des_ip(packet.src_ip());
                    packet.set_des_port(packet.src_port());
                    packet.set_src_ip(local_ip_);
                    packet.set_src_port(local_port_);
                    packet.set_ack_flag();

                    packet.clear_data();
                }
                else if(packet.is_recv_ack_packet()) {

                }
                else if(packet.is_send_data_packet()) {
                    packet.set_src_ip(local_ip_);
                    packet.set_src_port(local_port_);
                }
                else if(packet.is_send_ack_packet()) {
                    packet.set_src_ip(local_ip_);
                    packet.set_src_port(local_port_);
                }
                return true;
            }
        private:
            cortono::net::EventLoop* loop_;
            const std::string local_ip_;
            const std::uint16_t local_port_;
    };
}
