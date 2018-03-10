#pragma once

#include "../std.hpp"

namespace cortono::util
{
    class ci_less
    {
        public:
            bool operator()(const std::string& s1, const std::string& s2) {
                return std::lexicographical_compare(
                    s1.begin(), s1.end(), s2.begin(), s2.end(),
                    nocase_compare());
            }

            bool operator()(std::string_view s1, std::string_view s2) {
                return std::lexicographical_compare(
                    s1.begin(), s1.end(), s2.begin(), s2.end(),
                    nocase_compare());
            }

        private:
            struct nocase_compare {
                public:
                    bool operator()(char ch1, char ch2) {
                        return std::tolower(ch1) < std::tolower(ch2);
                    }
            };
    };

    inline bool iequal(const std::string& s1, const std::string& s2) {
        if(s1.size() != s2.size()) {
            return false;
        }
        for(std::size_t i = 0; i < s1.size(); ++i) {
            if(std::tolower(s1[i]) != std::tolower(s2[i])) {
                return false;
            }
        }
        return true;
    }
}
