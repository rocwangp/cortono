#pragma once

#include "../cortono.hpp"
#include "entry.hpp"
#include "timer.hpp"
#include "configuration.hpp"
#include "manager.hpp"

namespace p2p {

template <typename Host>
class Session : cortono::util::noncopyable {
public:
    using PTR = std::shared_ptr<Session>;

    Session(Host* host, cortono::net::TcpConnection::Pointer conn, Configuration& configuration, bool with_server = false)
        : host_(host),
          conn_(std::move(conn))
        , configuration_(configuration)
        , with_server_(with_server)
    {
        log_debug("create new session ", conn_->name());
        if(configuration_.heartbeat_timeout > 0) {
            heartbeat_timer_.enable(conn_->loop(), std::chrono::milliseconds(configuration_.heartbeat_timeout), [this] {
                if(!pong_recved_) {
                    log_error("lose connection in", conn_->name(), "close...");
                    conn_->close();
                }
                else {
                    ping();
                    pong_recved_ = false;
                }
            });
        }
    }
    ~Session() {
        // log_info << name() << "destroyed...";
    }
    static std::string name(const std::string& ip, std::uint16_t port) { return cortono::util::format("%s:%u", ip.data(), port); }
    std::string name() const { return name(ip(), port()); }
    std::string ip() const { return conn_->peer_ip(); };
    std::uint16_t port() const { return conn_->peer_port(); }
    std::pair<std::string, std::uint16_t> peer_endpoint() const { return conn_->peer_endpoint(); }

    bool with_server() const { return with_server_; }

    void handle_read() {
        context_.append(conn_->recv_all());
        while(parse_datagram()) {
            auto [ip, port] = datagram_->endpoint();
            switch(datagram_->type()) 
            {
                case AddrDatagram::type:
                    log_info(cortono::util::format("receive AddrDatagram(%s:%u) from %s", ip.data(), port, conn_->name().data()));
                    handle_addr_datagram();
                    break;
                case GetAddrDatagram::type:
                    log_info(cortono::util::format("receive GetAddrDatagram(%s:%u) from %s", ip.data(), port, conn_->name().data()));
                    handle_getaddr_datagram();
                    break;
                case PingDatagram::type:
                    log_info(cortono::util::format("receive PingDatagram from %s, send PongDatagram", conn_->name().data()));
                    handle_ping_datagram();
                    break;
                case PongDatagram::type:
                    log_info(cortono::util::format("receive PongDatagram from %s, reset heartbeat_timer", conn_->name().data()));
                    handle_pong_datagram();
                    break;
                default:
                    log_error("unknown datagram type...");
                    break;
            }
            datagram_.reset(nullptr);
        }
    }
    void send_datagram(const Datagram& datagram) { 
        std::weak_ptr<cortono::net::TcpConnection> weak_conn = conn_;
        conn_->loop()->safe_call([datagram, weak_conn]() { 
            if(auto conn = weak_conn.lock(); conn) {
                conn->send(datagram.serialize()); 
            }
            else {
                log_info("connection has destryoed, ignore send datagram...");
            }
        });
    }
private:
    void ping() { send_datagram(PingDatagram{}); }
    void pong() { send_datagram(PongDatagram{}); }

    bool parse_datagram() {
        if((!datagram_) && context_.length() >= Datagram::HEADER_LENGTH) {
            Datagram::DatagramHeader header = Datagram::parse_datagram_header(context_);
            datagram_.reset(new Datagram(std::move(header)));
            context_ = context_.substr(Datagram::HEADER_LENGTH);
        }
        if(datagram_ && context_.length() >= datagram_->body_size()) {
            datagram_->parse_datagram_body(context_);
            context_ = context_.substr(datagram_->body_size());
            return true;
        }
        return false;
    }
    void handle_addr_datagram() {
        auto [ip, port] = datagram_->endpoint();
        host_->connect_peer(ip, port);
        // host_->boardcast_to_network(*datagram_, conn_->name());
    }
    void handle_getaddr_datagram() {
        auto peers_list = host_->get_all_peers();
        std::vector<typename decltype(peers_list)::value_type> peers(peers_list.size());
        std::move_backward(peers_list.begin(), peers_list.end(), peers.end());
        std::shuffle(peers.begin(), peers.end(), std::mt19937{ std::random_device{}() });
        for(auto& peer : peers) {
            log_debug << peer.ip << " " << peer.port;
        }

        std::size_t send_count{ 0 };
        for(auto&& peer : peers) {
            if(peer.ip == ip() && peer.port == port()) {
                continue;
            }
            if(peer.ip == datagram_->ip() && peer.port == datagram_->port()) {
                continue;
            }
            if(configuration_.is_seed_peer(peer.ip, peer.port)) {
                continue;
            }
            if(send_count++ >= configuration_.max_send_addr_count) {
                break;
            }
            log_info(cortono::util::format("send AddrDatagram(%s:%u) by connection(%s)...", peer.ip.data(), peer.port, conn_->name().data()));
            send_datagram(AddrDatagram{ peer.ip, peer.port });
        }
        // for(auto&& [ip, port] : addresses) {
            // if(ip == this->ip() && port == this->port()) {
                // continue;
            // }
            // log_info(cortono::util::format("send AddrDatagram(%s:%u) by connection(%s)...", ip.data(), port, conn_->name().data()));
            // send_datagram(AddrDatagram{ ip, port });
        // }
    }
    void handle_ping_datagram() {
        pong();
    }
    void handle_pong_datagram() {
        pong_recved_ = true;
        heartbeat_timer_.reset();
    }
private:
    Host* host_{ nullptr };
    cortono::net::TcpConnection::Pointer conn_{ nullptr };
    Configuration& configuration_;
    RepeatTimer heartbeat_timer_;
    bool pong_recved_{ true };
    bool with_server_{ false };

    std::string context_;
    std::unique_ptr<Datagram> datagram_{ nullptr };
};



template <typename Host>
class SessionManager : public Manager<Session<Host>, true> {
public:
    using parent_t = Manager<Session<Host>, true>;

    SessionManager() {
        this->parent_t::set_counter([](const typename parent_t::item_t& item) {
            return item->with_server();
        });
    }
    std::vector<std::pair<std::string, std::uint16_t>> get_all_addresses() const {
        auto sessions = this->parent_t::all();
        std::vector<std::pair<std::string, std::uint16_t>> results;
        for(auto session : sessions) {
            if(session->with_server()) {
                results.emplace_back(session->ip(), session->port());
            }
        }
        return results;
    }
    std::size_t server_session_size() const {
        return this->parent_t::count();
    }
};

} // namespace p2p
