#pragma once

#include "../std.hpp"
#include "../cortono.hpp"

#include <shared_mutex>

namespace p2p {

template <bool Pack, typename T>
struct PTR_TRAITS {
    using type = typename T::PTR;
};

template <typename T>
struct PTR_TRAITS<false, T> {
    using type = T;
};

template <typename T, bool Pack = false, bool Mutex = true>
class Manager {
public:
    using item_t = typename PTR_TRAITS<Pack, T>::type;

    void append(const item_t& item) {
        std::string name;
        if constexpr (Pack) {
            name = item->name();
        }
        else {
            name = item.name();
        }
        std::unique_lock ulock{ mutex_, std::defer_lock };
        if constexpr (Mutex) {
            ulock.lock();
        }
        if(!iters_.count(name)) {
            items_.emplace_back(item);
            iters_.emplace(name, --items_.end());
            if(pred_ && pred_(item)) {
                ++counter_;
            }
        }
    }
    template <typename... Args>
    typename std::enable_if_t<(sizeof...(Args) > 1), void> append(Args&&... args) {
        append(item_t(std::forward<Args>(args)...));
    }
    template <typename... Args>
    void remove(Args&&... args) {
        std::string name = T::name(std::forward<Args>(args)...);
        std::unique_lock ulock{ mutex_, std::defer_lock };
        if constexpr (Mutex) {
            ulock.lock();
        }
        if(auto it = iters_.find(name); it != iters_.end()) {
            if(pred_ && pred_(*(it->second))) {
                --counter_;
            }
            items_.erase(it->second);
            iters_.erase(it);
        }
        else {
            log_info("no item in manager...", name);
        }
    }
    template <typename... Args>
    bool exist(Args&&... args) const {
        std::shared_lock ulock{ mutex_, std::defer_lock };
        if constexpr (Mutex) {
            ulock.lock();
        }
        return iters_.count(T::name(std::forward<Args>(args)...));
    }
    template <typename... Args>
    item_t get(Args&&... args) const {
        std::string name = T::name(std::forward<Args>(args)...);
        std::shared_lock ulock{ mutex_, std::defer_lock };
        if constexpr (Mutex) {
            ulock.lock();
        }
        if(auto it = iters_.find(name); it != iters_.end()) {
            return *(it->second);
        }
        else {
            return item_t{};
        }
    }
    std::list<item_t> all() const {
        std::shared_lock ulock{ mutex_, std::defer_lock };
        if constexpr (Mutex) {
            ulock.lock();
        }
        return items_;
    }
    std::size_t count_if(std::function<bool(const item_t& item)>&& pred) const {
        std::shared_lock ulock{ mutex_, std::defer_lock };
        if constexpr (Mutex) {
            ulock.lock();
        }
        return std::count_if(items_.begin(), items_.end(), std::move(pred));
    }
    std::size_t count() const {
        return counter_;
    }
    void set_counter(std::function<bool(const item_t&)>&& pred) {
        pred_ = std::move(pred);
    }

protected:
    mutable std::shared_mutex mutex_;
    std::list<item_t> items_;
    std::unordered_map<std::string, typename std::list<item_t>::iterator> iters_;

    std::atomic<std::size_t> counter_{ 0 };
    std::function<bool(const item_t&)> pred_{ nullptr };
};

} // namespace p2p
