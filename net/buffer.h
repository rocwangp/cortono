#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>


namespace cortono
{
    namespace net
    {
        template <std::size_t Size>
        class BufferBase
        {
            public:
                BufferBase()
                    : read_idx_(0),
                      write_idx_(0),
                      buffer_(Size)
                {  }

                char* begin() { return &buffer_[read_idx_]; }
                char* end() { return &buffer_[write_idx_]; }
                bool empty()  noexcept { return read_idx_ == write_idx_; }
                int size()  { return write_idx_ - read_idx_; }

                int readable() { return read_idx_; }
                int writeable() { return buffer_.size() - write_idx_; }

                void append_bytes(int bytes) { write_idx_ += bytes; }

                void append(std::string info)
                {
                    int s = info.size();
                    enable_bytes(s);
                    std::move_backward(info.begin(), info.end(), end() + s);
                    append_bytes(s);
                }

                void append(int n)
                {
                    append(std::to_string(n));
                }

                void append(char *str)
                {
                    append(std::string(str));
                }
                void enable_bytes(int bytes)
                {
                    if(writeable() < bytes)
                    {
                        std::move_backward(begin(), end(), buffer_.begin());
                        write_idx_ = size();
                        read_idx_ = 0;
                        if(writeable() >= bytes)
                            return;
                        buffer_.resize(buffer_.size() + bytes);
                    }
                }

                std::string read_all()
                {
                    std::string info(begin(), end());
                    read_idx_ = write_idx_ = 0;
                    return info;
                }
            private:
                std::size_t read_idx_, write_idx_;
                std::vector<char> buffer_;
        };

        class Buffer : public BufferBase<1024>
        {

        };
    }
}
