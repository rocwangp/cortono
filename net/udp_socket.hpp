#pragma once

#include "../std.hpp"
#include "../ip/sockets.hpp"
#include "socket.hpp"

namespace cortono::net
{
    class UdpSocket : public TcpSocket
    {
        public:
            UdpSocket(int server_fd,
                      const std::string& server_ip, std::uint16_t server_port,
                      const std::string& client_ip, std::uint16_t client_port)
                : TcpSocket(server_fd),
                  server_ip_(server_ip),
                  server_port_(server_port),
                  client_ip_(client_ip),
                  client_port_(client_port)

            {  }

            int send(const char* buffer, int len) {
                return ip::udp::sockets::send(fd_, buffer, len, client_ip_, client_port_);
            }
            int recv(char* buffer, int len) {
                std::string ip;
                std::uint16_t port;
                return ip::udp::sockets::recv(fd_, buffer, len, ip, port);
            }
        private:
            std::string server_ip_;
            std::uint16_t server_port_;
            std::string client_ip_;
            std::uint16_t client_port_;
    };
}
