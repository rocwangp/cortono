#pragma once


#include "../std.hpp"
#include "../util/util.hpp"

namespace cortono::net
{
    template <std::size_t Size>
    class BaseBuffer
    {
        public:
            BaseBuffer()
                : read_idx_(0),
                  write_idx_(0),
                  buffer_(Size)
            {  }

            const char* data() const {
                return &buffer_[read_idx_];
            }

            char* begin() {
                return &buffer_[read_idx_];
            }
            char* end() {
                return &buffer_[write_idx_];
            }
            bool empty()  noexcept {
                return read_idx_ == write_idx_;
            }
            int size()  {
                if(read_idx_ > write_idx_) {
                    log_fatal("buffer error:", read_idx_, write_idx_);
                }
                return write_idx_ - read_idx_;
            }
            int readable() {
                return read_idx_;
            }
            int writeable() {
                return buffer_.size() - write_idx_;
            }

            void clear() {
                read_idx_ = write_idx_ = 0;
            }

            void retrieve_read_bytes(int bytes) {
                read_idx_ += bytes;
                if(read_idx_ == write_idx_) {
                    clear();
                }
            }

            void retrieve_write_bytes(int bytes) {
                write_idx_ += bytes;
            }

            void append(std::string info) {
                int s = info.length();
                enable_bytes(s);
                std::move_backward(info.begin(), info.end(), end() + s);
                retrieve_write_bytes(s);
            }

            void append(int n) {
                append(std::to_string(n));
            }

            void append(char *str) {
                append(std::string(str));
            }
            void append(const char* s, int len) {
                append(std::string(s, len));
            }
            void enable_bytes(int bytes) {
                if(writeable() < bytes) {
                    std::memmove(&buffer_[0], &buffer_[read_idx_], size());
                    // std::move_backward(first, last, d_last); (d_last - 1)是最后一个元素的位置
                    // std::move_backward(begin(), end(), buffer_.begin() + size() - 1); error
                    // std::move_backward(begin(), end(), buffer_.begin() + size()); ok
                    // 如果d_last在[first, last)之间，则行为未定义
                    write_idx_ = size();
                    read_idx_ = 0;
                    if(writeable() >= bytes)
                        return;
                    buffer_.resize(buffer_.size() + bytes - writeable());
                }
            }

            std::string read_all() {
                std::string info(data(), size());
                read_idx_ = write_idx_ = 0;
                return info;
            }
            std::string read_bytes(int n) {
                n = std::min(n, size());
                std::string info(data(), n);
                retrieve_read_bytes(n);
                return info;
            }

            std::string read_util(std::string_view s) {
                log_trace;
                if(auto search_it = std::search(begin(), end(), s.begin(), s.end());
                        search_it != end())
                {
                    log_debug(*search_it);
                    std::string info(begin(), search_it + s.length());
                    log_debug(info);
                    read_idx_ += info.length();
                    return info;
                }
                else
                {
                    log_error("no match string in buffer:", s, "call read_all()");
                    return read_all();
                }
            }

            std::string_view read_sv_util(std::string_view s) {
                if(auto search_it = std::search(begin(), end(), s.begin(), s.end());
                        search_it != end()) {
                    return { begin(), search_it - begin() + s.length() };
                }
                else{
                    return {};
                }
            }
            std::string_view read_string_view() {
                if(empty()) {
                    return {};
                }
                else {
                    return { data(), static_cast<std::size_t>(size()) };
                }
            }
            std::string to_string() {
                return std::string(data(), size());
            }
        private:
            std::size_t read_idx_, write_idx_;
            std::vector<char> buffer_;
    };

    class Buffer : public BaseBuffer<2048>
    {

    };
 }
