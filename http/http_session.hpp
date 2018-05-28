#pragma once

#include "http_cookie.hpp"
namespace cortono::http
{
    class Session
    {
        public:
            Session(const std::string& name, const std::string& uuid_str, std::int32_t expire,
                const std::string& path = "/", const std::string& domain = "") {
                id_ = uuid_str;
                expire_ = expire == -1 ? 600 : expire;
                std::time_t now = std::time(nullptr);
                time_stamp_ = expire_ + now;
                cookie_.set_name(name);
                cookie_.set_path(path);
                cookie_.set_domain(domain);
                cookie_.set_value(uuid_str);
                cookie_.set_version(0);
                cookie_.set_max_age(expire == -1 ? -1 : time_stamp_);
            }
            const Cookie& get_cookie() {
                return cookie_;
            }

            void set_data(const std::string& key, std::any value) {
                data_[key] = value;
            }
            template <typename T>
            T get_data(const std::string& key) {
                auto it = data_.find(key);
                return (it != data_.end()) ? std::any_cast<T>(it->second) : T{};
            }
        private:
            Cookie cookie_;

            std::string id_;
            std::int32_t expire_;
            std::time_t time_stamp_;

            std::unordered_map<std::string, std::any> data_;
    };
}
