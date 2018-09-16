#pragma once

#include "../std.hpp"
#include <shared_mutex>

namespace p2p {

template <typename Key, typename Value>
class threadsafe_unordered_map {
public:
    using key_type = Key;
    using value_type = Value;

    template <typename... Args>
    void emplace(Args&&... args) {
        std::unique_lock lock { mutex_ };
        container_.emplace(std::forward<Args>(args)...);
    }
    value_type& operator[](const key_type& key) {
        std::unique_lock lock { mutex_ };
        return container_[key];
    }
    typename std::unordered_map<key_type, value_type>::iterator find(const key_type& key) {
        std::shared_lock lock { mutex_ };
        return container_.find(key);
    }
    std::size_t count(const key_type& key) {
        std::shared_lock lock { mutex_ };
        return container_.count(key);
    }
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<key_type, value_type> container_;
};

} // namespace p2p
