#pragma once

#include "../std.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>


namespace p2p {

struct Configuration {
    Configuration(std::string_view filepath) {
        boost::property_tree::ptree pt;
        boost::property_tree::xml_parser::read_xml(filepath.data(), pt);

        auto seed_nodes_conf = pt.get_child("p2pconfig.seed_nodes");
        for(auto& seed_node : seed_nodes_conf) {
            seed_nodes.emplace_back(seed_node.second.get<std::string>("ip"), seed_node.second.get<std::uint16_t>("port"));
        }

        local_ip = pt.get<std::string>("p2pconfig.local_ip");
        local_port = pt.get<std::uint16_t>("p2pconfig.local_port");

        concurrency = pt.get<std::size_t>("p2pconfig.concurrency");
        max_connection_count = pt.get<std::size_t>("p2pconfig.max_connection_count");
        max_send_addr_count = pt.get<std::size_t>("p2pconfig.max_send_addr_count");
        heartbeat_timeout = pt.get<std::uint32_t>("p2pconfig.heartbeat_timeout");
    }
    bool is_seed_peer() const {
        return is_seed_peer(local_ip, local_port);
    }
    bool is_seed_peer(const std::string& ip, std::uint16_t port) const {
        return std::find(seed_nodes.begin(), seed_nodes.end(), std::make_pair(ip, port)) != seed_nodes.end();
    }
    std::string local_ip;
    std::uint16_t local_port;
    std::vector<std::pair<std::string, std::uint16_t>> seed_nodes;

    std::size_t concurrency;
    std::size_t max_connection_count;
    std::size_t max_send_addr_count;
    std::uint32_t heartbeat_timeout;
};

} // namespace p2p
