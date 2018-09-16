#pragma once

#include "../std.hpp"
#include "../cortono.hpp"

#include "manager.hpp"

namespace p2p {



struct Peer {
    std::string ip;
    std::uint16_t port;

    Peer(const std::string& i = "", std::uint16_t p = 0) : ip(i), port(p) {}
    std::string name() const { return name(ip, port); }
    static std::string name(const std::string& i, std::uint16_t p) { return cortono::util::format("%s:%u", i.data(), p) ; }
};

inline bool operator==(const Peer& peer1, const Peer& peer2) { return peer1.ip == peer2.ip && peer1.port == peer2.port; }

struct PeerHasher {
    std::size_t operator()(const Peer& peer) const {
        return std::hash<std::size_t>{}(std::hash<std::string>{}(peer.ip) + std::hash<std::uint16_t>{}(peer.port));
    }
};
class PeerManager : public Manager<Peer> {
public:
    using parent_t = Manager<Peer>;
};





} // namespace p2p
