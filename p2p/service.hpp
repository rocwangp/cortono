#pragma once

#include "../cortono.hpp"

#include "configuration.hpp"
#include "session.hpp"
#include "peer.hpp"

namespace p2p {

class Service : cortono::util::noncopyable {
public:
    using SessionType = Session<Service>;
    using SessionManagerType = SessionManager<Service>;

    explicit Service(const Configuration& configuration);

    void run();
    void boardcast_to_network(const Datagram& datagram, const std::string& filter_name = "");
    bool connect_peer(const std::string& ip, std::uint16_t port);

    std::list<typename SessionManagerType::item_t> get_all_sessions() { return session_manager_.all(); }
    std::list<typename PeerManager::item_t> get_all_peers() { return peer_manager_.all(); }
    std::vector<std::pair<std::string, std::uint16_t>> get_all_addresses() { return session_manager_.get_all_addresses(); }

    bool is_connected_peer(const std::string& ip, std::uint16_t port) { return peer_manager_.exist(ip, port); }
private:
    void init_callback();
    void connect_seed_peers();

    // called in conn->loop() thread
    void handle_close(const cortono::net::TcpConnection::Pointer& conn);

    // called in conn->loop() thread
    void handle_read(const cortono::net::TcpConnection::Pointer& conn) ;

    // save connections when handshaking to prevent it is destroyed
    // maybe other thread call append_tp_connectings at same time, locking for threadsafe
    void append_to_connectings(cortono::net::TcpConnection::Pointer& conn);

    // when connection handshakes done or error, remove it... locking for threadsafe
    void remove_from_connectings(const cortono::net::TcpConnection::Pointer& conn) ;

    // find available peer from peer_manager_ and try to connect
    void maintain_connection_count();

    void print_connected_address();

    bool is_local_peer(const std::string& ip, std::uint16_t port) const {
        return ip == configuration_.local_ip && port == configuration_.local_port;
    }
    bool is_connected_session(const std::string& ip, std::uint16_t port) {
        return session_manager_.exist(ip, port);
    }
    bool is_allowed_peer(const std::string& ip, std::uint16_t port) const {
        return ip != "0.0.0.0" && port != 0;
    }
private:
    cortono::net::EventLoop loop_;
    cortono::net::TcpService service_;

    Configuration configuration_;

    SessionManagerType session_manager_;
    PeerManager peer_manager_;

    std::mutex mutex_;

    std::unordered_map<std::string, cortono::net::TcpConnection::Pointer> connectings_;
};
} // namespace p2p
