#pragma once

#include "../std.hpp"
#include "http_utils.hpp"

namespace cortono::http
{
    struct ci_hash
    {
        std::size_t operator()(std::string s) const {
            utils::to_lower(s);
            return std::hash<std::string>{}(s);
        }
    };

    struct ci_key_compare
    {
        bool operator()(const std::string& s1, const std::string& s2) const {
            if(s1.length() != s2.length()) {
                return false;
            }
            for(std::size_t i = 0; i != s1.length(); ++i) {
                if(std::tolower(s1[i]) != std::tolower(s2[i])) {
                    return false;
                }
            }
            return true;
        }
    };

    using ci_map = std::unordered_map<std::string, std::string, ci_hash, ci_key_compare>;
}
