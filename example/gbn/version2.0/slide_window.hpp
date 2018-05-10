#pragma once

#include "../../../std.hpp"

namespace cortono
{
    template <std::uint64_t BufferSize, std::uint64_t WindowSize>
    class SlideWindow
    {
        public:
            bool full() const {
                return readable_bytes_ == BufferSize;
            }
            // [start_index, end_index)
            bool in_valid_range(std::uint64_t start_index, std::uint64_t end_index) {
                end_index %= BufferSize;
                // 0  [win_left_, win_right_) BufferSize
                if(win_left_ < win_right_) {
                    // start_index  win_left_  win_right_
                    // win_left_  win_right_  end_index
                    if(start_index < win_left_ || end_index > win_right_) {
                        return false;
                    }
                    // win_left_  win_right_  start_index
                    else if(start_index >= win_right_) {
                        return false;
                    }
                    // end_index  win_left_  win_right_
                    else if(end_index <= win_left_) {
                        return false;
                    }
                    // win_left_ start_index  end_index  win_right_
                    else {
                        return true;
                    }
                }
                // 0  win_right_  win_left_  BufferSize
                else {
                    // win_right_  start_index  win_left_
                    if(start_index < win_left_ && start_index >= win_right_) {
                        return false;
                    }
                    // win_right_  end_index  win_left_
                    else if(end_index < win_left_ && end_index >= win_right_) {
                        return false;
                    }
                    // 0  win_right_  win_left_  start_index  end_index  BufferSize
                    // 0  end_index  end_right_  win_left_  start_index  BufferSize
                    // 0  start_index  end_index  win_right_  win_left_  BufferSize
                    else {
                        return true;
                    }
                }
            }
            bool set_flag_if_valid(const char* data, std::uint64_t start_index, std::uint64_t len) {
                if(!in_valid_range(start_index, (start_index + len) % BufferSize)) {
                    return false;
                }
                for(std::uint64_t i = 0; i != len; ++i) {
                    window_.set((start_index + i) % BufferSize);
                    buffer_[(start_index + i) % BufferSize] = data[i];
                    if(win_left_ == ((start_index + i) % BufferSize)) {
                        window_.reset(win_left_);
                        win_left_ = (win_left_ + 1) % BufferSize;
                        win_right_ = (win_right_ + 1) % BufferSize;
                        ++readable_bytes_;
                    }
                }
                log_info("window move to", win_left_, win_right_);
                return true;
            }
            bool move_if_valid(std::uint64_t end_index) {
                if(!in_valid_range(win_left_, end_index)) {
                    return false;
                }
                while(win_left_ != end_index) {
                    win_left_ = (win_left_ + 1) % BufferSize;
                    win_right_ = (win_right_ + 1) % BufferSize;
                }
                return true;
            }
            std::uint64_t window_size() const {
                if(win_left_ < win_right_) {
                    return win_right_ - win_left_;
                }
                else {
                    return BufferSize - win_left_ + win_right_;
                }
            }
            std::uint64_t left_bound() const {
                return win_left_;
            }
            std::uint64_t right_bound() const {
                return win_right_;
            }
            std::uint64_t readable() const {
                /* if(read_idx_ <= win_left_) { */
                /*     return win_left_ - read_idx_; */
                /* } */
                /* else { */
                /*     return BufferSize - read_idx_ + win_left_; */
                /* } */
                return readable_bytes_;
            }
            std::string recv_all() {
                auto n = readable();
                if(n == 0) {
                    return "";
                }
                log_info("readable bytes:", n);
                std::string info;
                info.reserve(n);
                if(read_idx_ <= win_left_) {
                    info.append(&buffer_[read_idx_], n);
                }
                else {
                    info.append(&buffer_[read_idx_], BufferSize - read_idx_);
                    info.append(&buffer_[0], win_left_);
                }
                read_idx_ = win_left_;
                readable_bytes_ -= n;
                return info;
            }


            SlideWindow() {}
            SlideWindow(const SlideWindow& other)
                : win_left_(other.win_left_),
                  win_right_(other.win_right_),
                  read_idx_(other.read_idx_),
                  readable_bytes_(other.readable_bytes_),
                  window_(other.window_),
                  buffer_(other.buffer_)
            { }

            SlideWindow(SlideWindow&& other)
                : win_left_(std::move(other.win_left_)),
                  win_right_(std::move(other.win_right_)),
                  read_idx_(std::move(other.read_idx_)),
                  readable_bytes_(std::move(other.readable_bytes_)),
                  window_(std::move(other.window_)),
                  buffer_(std::move(other.buffer_))
            { }

            SlideWindow& operator=(const SlideWindow& other) {
                if(this != &other) {
                    win_left_ = other.win_left_;
                    win_right_ = other.win_right_;
                    read_idx_ = other.read_idx_;
                    readable_bytes_ = other.readable_bytes_;
                    window_ = other.window_;
                    buffer_ = other.buffer_;
                }
                return *this;
            }
            SlideWindow&& operator==(SlideWindow&& other) {
                SlideWindow tmp(std::move(other));
                std::swap(tmp, *this);
                return *this;
            }
        private:
            std::uint64_t win_left_{ 0 };
            std::uint64_t win_right_{ WindowSize };
            std::uint64_t read_idx_{ 0 };
            std::uint64_t readable_bytes_{ 0 };

            std::bitset<BufferSize> window_;
            std::array<char, BufferSize> buffer_;
    };
}
