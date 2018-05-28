#pragma once
#include "../std.hpp"
#include "http_session.hpp"
#include "http_uuid.hpp"

namespace cortono::http
{
    class session_manager
    {
    public:
        static std::shared_ptr<Session> create_session(
            const std::string& name, std::int32_t expire, const std::string& path = "/", const std::string& domain = "") {
            uuids::uuid_random_generator uid{};
            std::string uuid_str = uid().to_short_str();
            auto s = std::make_shared<Session>(name, uuid_str, expire, path, domain);
            {
                std::unique_lock lock { mutex_ };
                sessions_.emplace(std::move(uuid_str), s);
            }
            return s;
        }
        static std::shared_ptr<Session> create_session(
            std::string_view host, const std::string& name, std::int32_t expire = -1, const std::string& path = "/") {
            auto pos = host.find_first_of(':');
            if(pos != std::string_view::npos) {
                host = host.substr(0, pos);
            }
            return create_session(name, expire, path, std::string(host.data(), host.length()));
        }
        static std::weak_ptr<Session> get_session(const std::string& id) {
            std::unique_lock lock{ mutex_ };
            auto it = sessions_.find(id);
            return (it == sessions_.end()) ? nullptr : it->second;
        }
        static void remove_session(const std::string& id) {
            std::unique_lock lock{ mutex_ };
            sessions_.erase(id);
        }

    private:
        static std::mutex mutex_;
        static std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
    };

    std::mutex session_manager::mutex_;
    std::unordered_map<std::string, std::shared_ptr<Session>> session_manager::sessions_;
}
