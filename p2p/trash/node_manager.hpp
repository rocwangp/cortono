#pragma once

#include "../std.hpp"
#include "../cortono.hpp"

#include "node.hpp"

namespace p2p {


// manage all connected node
class NodeManager {
    public:
        NodeManager() = default;

        void append_node(const std::shared_ptr<Node>& node) {
            nodes_.emplace_back(node);
            // nodes_index_[to_node_name(node->ip(), node->port())] = nodes_.size() - 1;
            // log_debug("append node(", node->ip(), node->port(), ")", to_node_name(node->ip(), node->port()));
        }
        void remove_node(const std::string& ip, std::uint16_t port) {
            for(auto it = nodes_.begin(); it != nodes_.end(); ++it) {
                if((*it)->ip() == ip && (*it)->port() == port) {
                    nodes_.erase(it);
                    break;
                }
            }
            // if(auto iter = nodes_index_.find(to_node_name(ip, port)); iter != nodes_index_.end()) {
                // nodes_.erase(nodes_.begin() + iter->second);
                // nodes_index_.erase(iter);
            // }
        }
        std::shared_ptr<Node> acquire_node(const std::string& ip, std::uint16_t port) {
            for(auto it = nodes_.begin(); it != nodes_.end(); ++it) {
                if((*it)->ip() == ip && (*it)->port() == port) {
                    return *it;
                }
            }
            return nullptr;
            // if(auto iter = nodes_index_.find(to_node_name(ip, port)); iter != nodes_index_.end()) {
                // return nodes_[iter->second];
            // }
            // throw std::runtime_error("failed to acquire node, name: " + to_node_name(ip, port) + " not exist...");
        }
        const std::vector<std::shared_ptr<Node>>& get_all_nodes() const {
            return nodes_;
        }
        bool exist(const std::string& ip, std::uint16_t port) const {
            return std::find_if(nodes_.begin(), nodes_.end(), [&](const auto& node_ptr) { 
                return node_ptr->ip() == ip && node_ptr->port() == port; 
            }) != nodes_.end();
            // return nodes_index_.find(to_node_name(ip, port)) != nodes_index_.end();
        }
        std::size_t size() const {
            return nodes_.size();
        }
    private:
        std::string to_node_name(const std::string& ip, std::uint16_t port) const {
            return ip + ":" + std::to_string(port);
        }
    private:
        // std::unordered_map<std::string, std::size_t> nodes_index_;
        std::vector<std::shared_ptr<Node>> nodes_;
};
} // namespace p2p
