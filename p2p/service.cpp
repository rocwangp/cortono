#include "service.hpp"

namespace p2p {


Service::Service(const Configuration& configuration)
    : service_(&loop_, configuration.local_ip, configuration.local_port)
    , configuration_(configuration)
{ }

void Service::run() {
    service_.on_conn([this](auto conn) {
        log_debug("accept connection from ", conn->name());
        conn->on_read(std::bind(&Service::handle_read, this, std::placeholders::_1));
        conn->on_close(std::bind(&Service::handle_close, this, std::placeholders::_1));
        session_manager_.append(std::make_shared<SessionType>(this, conn, configuration_, false));
        print_connected_address();
    });
    service_.start_threadpool(configuration_.concurrency);
    connect_seed_peers();
    service_.start_acceptor();

    log_info(cortono::util::format("(%s:%u), start done...", configuration_.local_ip.data(), configuration_.local_port));
    loop_.loop();
}

// try to connect peer(ip, port) and record peer to peer_manager, called when 
// 1. init local service 
// 2. recieve AddrDatagram
bool Service::connect_peer(const std::string& ip, std::uint16_t port) {
    if(!is_allowed_peer(ip, port)) {
        log_error(cortono::util::format("(%s:%u) is invalid peer, ignore...", ip.data(), port));
        return false;
    }
    if(is_connected_session(ip, port)) {
        log_error(cortono::util::format("(%s:%u) has connected, ignore...", ip.data(), port));
        return false;
    }
    if(is_local_peer(ip, port)) {
        log_error(cortono::util::format("(%s:%u) is local peer, ignore...", ip.data(), port));
        return false;
    }
    
    // record peer
    peer_manager_.append(ip, port);

    {
        std::unique_lock lock { server_session_size_mutex_ };
        if(server_session_size_ >= configuration_.max_connection_count) {
            log_info(cortono::util::format("connection count(%d) is enough", server_session_size_));
            return false;
        }
        ++server_session_size_;
    }
    auto loop = service_.acquire_eventloop();
    loop->safe_call([=]() {
        auto read_cb = std::bind(&Service::handle_read, this, std::placeholders::_1);
        auto close_cb = std::bind(&Service::handle_close, this, std::placeholders::_1);
        auto error_cb = [=](const auto& conn) { this->handle_error(conn, ip, port); };
        auto conn_cb = [=](const auto& conn) { // called in conn->loop() thread
            remove_from_connectings(conn);
            auto session = std::make_shared<SessionType>(this, conn, configuration_, true);
            session_manager_.append(session);

            print_connected_address();

            // threadsafe
            log_debug << "handshake done... ";

            if(configuration_.is_seed_peer()) {
                log_debug << "local peer is seed peer, doesn\'t need to send (Get)AddrDatagram...";
                return;
            }
            log_debug << cortono::util::format("send AddrDatagram(%s:%u) ", configuration_.local_ip.data(), configuration_.local_port)
                      << cortono::util::format("to peer(%s:%u)", ip.data(), port);
            session->send_datagram(AddrDatagram{ configuration_.local_ip, configuration_.local_port });

            log_debug << cortono::util::format("send GetAddrDatagram(%s:%u) to peer(%s:%u)", configuration_.local_ip.data(), configuration_.local_port, ip.data(), port);
            session->send_datagram(GetAddrDatagram{ configuration_.local_ip, configuration_.local_port });
        };

        // XXX note: client doesn't belong to this thread
        auto client = cortono::net::TcpClient::connect(
            loop, ip, port, std::move(read_cb), nullptr, std::move(close_cb), std::move(conn_cb), std::move(error_cb));

        if(!client->is_connected()) {
            append_to_connectings(client);
        }
    });

    return true;
}
void Service::boardcast_to_network(const Datagram& datagram, const std::string& filter_name) {
    for(const auto& session : get_all_sessions()) {
        if(session->with_server() && session->name() != filter_name && 
            !(session->ip() == datagram.ip() && session->port() == datagram.port())) {
            log_debug(cortono::util::format("boardcast %s(%s:%u) by connection(%s)", 
                    type_to_name(datagram).data(), datagram.ip().data(), datagram.port(), session->name().data()));
            session->send_datagram(datagram);
        }
    }
}
void Service::connect_seed_peers() {
    for(const auto& [ip, port] : configuration_.seed_nodes) {
        if(is_local_peer(ip, port)) {
            log_info(cortono::util::format("<%s:%u> is local address, ignore...", ip.data(), port));
            continue;
        }
        log_info(cortono::util::format("try to connect to seed node(%s:%u)...", ip.data(), port));
        connect_peer(ip, port);
    }
}

// called in conn->loop() thread
void Service::handle_read(const cortono::net::TcpConnection::Pointer& conn) {
    auto [ip, port] = conn->peer_endpoint();
    auto session = session_manager_.get(ip, port);
    if(!session) {
        log_error << cortono::util::format("cannot find session(%s), create a new session and continue handle", conn->name().data());
        session = std::make_shared<Session<Service>>(this, conn, configuration_);
        session_manager_.append(session);
    }
    session->handle_read();
    print_connected_address();
}
void Service::handle_close(const cortono::net::TcpConnection::Pointer& conn) {
    log_info("close connection", conn->name());
    
    auto [peer_ip, peer_port] = conn->peer_endpoint();
    log_info(cortono::util::format("remove session(%s:%u)", peer_ip.data(), peer_port));
    session_manager_.remove(peer_ip, peer_port);

    auto [local_ip, local_port] = conn->local_endpoint();

    if(!is_local_peer(local_ip, local_port)) {
        peer_manager_.remove(peer_ip, peer_port);
        {
            std::unique_lock lock { server_session_size_mutex_ };
            --server_session_size_;
        }
        maintain_connection_count();
    }
    print_connected_address();
}
void Service::handle_error(const cortono::net::TcpConnection::Pointer& conn, const std::string& ip, std::uint16_t port) {
    log_info("handshake error for connection: ", conn->name());
    remove_from_connectings(conn);
    peer_manager_.remove(ip, port);
    {
        std::unique_lock lock { server_session_size_mutex_ };
        --server_session_size_;
    }
    maintain_connection_count();
    print_connected_address();
}

void Service::maintain_connection_count() {
    auto peers = peer_manager_.all();
    for(auto&& peer : peers) {
        if(connect_peer(peer.ip, peer.port)) {
            return;
        }
    }
    {
        std::shared_lock lock { server_session_size_mutex_ };
        if(server_session_size_ >= configuration_.max_connection_count) {
            return;
        }
    }
    if(!configuration_.is_seed_peer()) {
        log_info("fail to connect peer from peer_manager, start boardcast...");
        boardcast_to_network(GetAddrDatagram{ configuration_.local_ip, configuration_.local_port });
    }
}

// save connections when handshaking to prevent it is destroyed
// maybe other thread call append_tp_connectings at same time, locking for threadsafe
void Service::append_to_connectings(cortono::net::TcpConnection::Pointer& conn) {
    // local_ip:local_port can indicate unique client connection becasuse of TIME_WAIT
    auto [local_ip, local_port] = conn->local_endpoint();
    std::unique_lock lock{ mutex_ };
    connectings_.emplace(cortono::util::format("%s:%u", local_ip.data(), local_port), conn);
}

// when connection handshakes done or error, remove it... locking for threadsafe
void Service::remove_from_connectings(const cortono::net::TcpConnection::Pointer& conn) {
    auto [local_ip, local_port] = conn->local_endpoint();
    std::unique_lock lock{ mutex_ };
    if(auto it = connectings_.find(cortono::util::format("%s:%u", local_ip.data(), local_port)); it != connectings_.end()) {
        connectings_.erase(it);
        log_debug(cortono::util::format("remove connection(%s:%u) from connectings_", local_ip.data(), local_port));
    }
    else {
        log_debug(cortono::util::format("cannot find connection(%s:%u) from connectings_", local_ip.data(), local_port));
    }
}
void Service::print_connected_address() const {
    log_debug << "all connected address is...";
    for(const auto& [ip, port] : session_manager_.get_all_addresses()) {
        if(is_local_peer(ip, port)) {
            continue;
        }
        log_debug(cortono::util::format("%s:%u", ip.data(), port));
    }
}
}
