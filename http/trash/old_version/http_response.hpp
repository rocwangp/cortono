#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "response_cv.hpp"
#include "http_codec.hpp"

namespace cortono::http
{
    using namespace cortono::util;
    class HttpResponse
    {
        public:
            template <typename H, typename T>
            void set_header(H key, T value) {
                std::string_view header_key = ResponseHeader_to_sv(key);
                std::string header_value = to_chars(value);
                headers_.emplace(header_key, header_value);
            }
            void set_status_and_content(ResponseStatus status) {
                set_status_and_content(status, status_to_content(status));
            }
            void set_status_and_content(ResponseStatus status, std::string_view content) {
                status_ = status;
                content_ = (std::string{ content.data(), content.length() });
            }
            std::string get_response_header() {
                std::string_view status_sv = status_to_sv(status_);
                std::string response{ status_sv.data(), status_sv.length() };
                for(auto &[key, value] : headers_) {
                    response.append(key);
                    response.append(key_value_spacer);
                    response.append(value);
                    response.append(crlf);
                }
                response.append(crlf);
                return response;
            }
            std::string to_string() {
                set_header(ResponseHeader::Content_Length, content_.size());
                std::string r =  get_response_header() + content_;
                /* log_debug(r); */
                return r;
            }
            void gzip_file(std::string_view file_path) {
                content_ = gzip_codec::compress(file_path);
            }
            void reset() {
                content_.clear();
                headers_.clear();
                status_ = ResponseStatus::OK;
            }

        private:
            ResponseStatus status_;
            std::string content_;
            std::unordered_map<std::string_view, std::string> headers_;
    };
}
