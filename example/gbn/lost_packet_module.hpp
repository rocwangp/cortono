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

            // nothing to check for this module
            template <typename Parser>
            bool check(Parser& ) {
                return true;
            }

            // rand to simulate packet loss by make error on packet then
            // other module will find out the error and don't carry out anything
            template <typename Parser>
            bool handle(Parser& parser) {
                if(parser.is_recv_data_packet()) {
                    if(std::rand() % 60000 < 60000 * Num / Denom) {
                        // if the src_ip equal to des_ip and the src_port equal to des_port
                        // the packet will be seen as the error packet
                        parser.set_error_packet();
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
