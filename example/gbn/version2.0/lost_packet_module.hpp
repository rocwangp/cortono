#pragma once

#include "../../../cortono.hpp"

namespace cortono
{
    template <std::uint16_t Num, std::uint16_t Denom>
    class LostPacketModule
    {
        public:
            LostPacketModule(cortono::net::EventLoop* loop, const std::string& ip, std::uint16_t port)
                : loop_(loop),
                  ip_(ip),
                  port_(port)
            {
                std::srand((unsigned short)(time(nullptr)));
            }

            template <typename Parser>
            bool check(Parser& ) {
                return true;
            }

            template <typename Parser>
            bool handle(Parser& parser) {
                if(parser.is_recv_data_packet()) {
                    if(std::rand() % 60000 < 60000 * Num / Denom) {
                        parser.set_src_port(port_);
                        parser.set_des_port(port_);
                        parser.set_control("000000");
                    }
                }
                return true;
            }

        private:
            cortono::net::EventLoop* loop_;
            std::string ip_;
            std::uint16_t port_;
    };
}
