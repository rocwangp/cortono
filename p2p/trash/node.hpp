#pragma once


#include "../cortono.hpp"
#include "entry.hpp"
#include "timer.hpp"

namespace p2p {


class Node {
    public:
        Node() {} 

        Node(const std::string& ip, std::uint16_t port, const cortono::net::TcpConnection::Pointer& conn = nullptr)
            : ip_(ip)
            , port_(port)
            , conn_(conn)
        { }

        ~Node() {
            log_info("node(", ip_, port_, ") destroyed");
            if(datagram_) {
                delete datagram_;
            }
            heartbeat_timer_.disable();
        }

        std::string ip() const { return ip_; }
        std::uint16_t port() const { return port_; }
    public:
        Datagram* parse_datagram() {
            if(!conn_->recv_buffer()->empty()) {
                recved_entry_.append(conn_->recv_all());
            }

            if(!datagram_header_done && recved_entry_.length() >= Datagram::header_length()) {
                Datagram::DatagramHeader header = Datagram::parse_datagram_header(recved_entry_);
                if(datagram_) {
                    delete datagram_;
                    datagram_ = nullptr;
                }
                switch(header.command) 
                {
                    case AddrDatagram::type:
                        // log_debug << "receive AddrDatagram...";
                        datagram_ = (new AddrDatagram(std::move(header)));
                        break;
                    case GetAddrDatagram::type:
                        // log_debug << "receive GetAddrDatagram...";
                        datagram_ = (new GetAddrDatagram(std::move(header)));
                        break;
                    case PongDatagram::type:
                        // log_debug << "receive PongDatagram...";
                        datagram_ = (new PongDatagram(std::move(header)));
                        break;
                    case PingDatagram::type:
                        // log_debug << "receive PingDatagram...";
                        datagram_ = (new PingDatagram(std::move(header)));
                        break;
                    default:
                        log_error << "unknown Datagram...";
                        break;
                }
                recved_entry_ = recved_entry_.substr(Datagram::header_length());
                datagram_header_done = true;
            }
            if(datagram_header_done && recved_entry_.length() >= datagram_->body_size()) {
                datagram_->parse_datagram_body(recved_entry_);
                recved_entry_ = recved_entry_.substr(datagram_->body_size());
                datagram_header_done = false;
                return datagram_;
            }
            return nullptr;
        }
        void enable_heartbeat_timer(std::chrono::milliseconds timeout) {
            heartbeat_timer_.enable(conn_->loop(), timeout, [this]() {
                if(!heartbeat_responsed_) {
                    log_error("lose connection with:", conn_->name(), "close connection...");
                    heartbeat_timer_.disable();
                    conn_->close();
                }
                else {
                    log_info("send PingDatagram to peer(", conn_->name(), ") to maintain connection...");
                    send_datagram_to_peer(PingDatagram{});
                    heartbeat_responsed_ = false;
                }
            });
        }
        void reset_heartbeat_timer() {
            heartbeat_responsed_ = true;
            heartbeat_timer_.reset();
        }
        void send_datagram_to_peer(const Datagram& datagram) {
            conn_->send(datagram.serialize());
        }
        void handle_read() {

        }
        void handle_close() {
            heartbeat_timer_.disable();
        }

        const cortono::net::TcpConnection::Pointer& connector() const {
            return conn_;
        }
        
    private:
        std::string ip_;
        std::uint16_t port_;
        std::string name_{ "" };

        bool datagram_header_done{ false };
        std::string recved_entry_;
        Datagram* datagram_{ nullptr };

        cortono::net::TcpConnection::Pointer conn_;

        bool heartbeat_responsed_{ true };
        RepeatTimer heartbeat_timer_;
};



} // namespace p2p
