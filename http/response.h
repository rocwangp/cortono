#pragma once

#include <string>
#include <map>
#include "utils.h"
#include "response_cv.h"

namespace cortono::http
{
    class response
    {
        public:
            std::string_view get_header_value(const std::string& key) const {
                if(auto it = headers_.find(key); it != headers_.end()) {
                    return { it->second.data(), it->second.length() };
                }
                return {};
            }

            void set_status_and_content(status_type status) {
                std::string content { status_to_sv(status) };
                set_status_and_content(status, std::move(content));
            }

            void set_status_and_content(status_type status, std::string&& content) {
                status_ = status;
                set_content(std::move(content));
            }

            void set_content(std::string&& content) {
                content_ = std::move(content);
                add_header("Content-Length", std::to_string(content_.size()));
            }

            void add_header(std::string&& key, std::string&& value) {
                headers_[std::move(key)] = std::move(value);
            }

            std::string to_string() {
                std::string info { status_to_sv(status_) };
                for(auto &[key, value] : headers_) {
                    info.append(key);
                    info.append(name_value_separator);
                    info.append(value);
                    info.append(crlf);
                }
                info.append(crlf);
                info.append(content_);
                return info;
            }
        private:
            std::map<std::string, std::string, ci_less> headers_;
            std::string content_;
            status_type status_ = status_type::init;
            std::string chunk_size_;

    };
}
