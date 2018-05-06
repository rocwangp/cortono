#pragma once

#include "../ip/sockets.hpp"

namespace cortono::net
{
    class UdpConnection
    {
        public:
            UdpConnection(int fd) : fd_(fd) {}

            void handle_read() {};
            void handle_write() {};
            void handle_close() {};
            void send_message(const std::string&, std::uint16_t, const std::string&) {}
        protected:
            int recv(char* buffer, int len, std::string& ip, std::uint16_t& port) {
                return cortono::ip::udp::sockets::recv(fd_, buffer, len, ip, port);
            }
            int send(const char* buffer, int len, const std::string& ip, std::uint16_t port) {
                return cortono::ip::udp::sockets::send(fd_, buffer, len, ip, port);
            }
        private:
            int fd_;
    };
}
