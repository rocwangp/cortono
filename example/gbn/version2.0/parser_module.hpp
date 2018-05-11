#pragma once

#include "sr_header.hpp"
#include "utility.hpp"

namespace cortono
{
    inline std::uint64_t conv_to_ten_digits(std::uint16_t s, const char* buffer, std::uint32_t len) {
        std::uint64_t res = 0;
        std::uint64_t base = 1;
        for(int i = len - 1; i >= 0; --i) {
            res += base * (buffer[i] - '0');
            base *= s;
        }
        return res;
    }
    inline std::string conv_to_string(std::uint16_t d, std::uint64_t n, std::uint32_t len) {
        std::string str;
        for(std::uint32_t i = 0; i != len; ++i) {
            str.append(1, '0' + n % d);
            n /= d;
        }
        std::reverse(str.begin(), str.end());
        return str;
    }

    template <typename T, std::uint64_t MaxDataSize>
    class MsgPacket
    {
        private:
            T seq_{ 0 };
            T ack_{ 0 };
            std::array<char, 6> control_ = { {'0', '0', '0', '0', '0', '0' } };
            std::uint16_t notice_size_{ 0 };
            std::uint16_t check_code_{ 0 };
            std::string message_;

            std::string src_ip_{ "127.0.0.1" };
            std::uint16_t src_port_{ 0 };

            std::string des_ip_{ "127.0.0.1" };
            std::uint16_t des_port_{ 0 };

            std::string local_ip_;
            std::uint16_t local_port_;
        public:
            enum {
                MIN_SIZE = //black_magic::bits_traits_v<decltype(src_port_)> +
                           //black_magic::bits_traits_v<decltype(des_port_)> +
                           black_magic::bits_traits_v<decltype(seq_)> +
                           black_magic::bits_traits_v<decltype(ack_)> +
                           std::tuple_size_v<decltype(control_)> +
                           black_magic::bits_traits_v<decltype(notice_size_)> +
                           black_magic::bits_traits_v<decltype(check_code_)>,

                MAX_SIZE = MIN_SIZE + MaxDataSize
            };

            MsgPacket(const std::string& local_ip, std::uint16_t local_port,
                      const std::string& src_ip, std::uint16_t src_port,
                      const std::string& des_ip, std::uint16_t des_port)
                : local_ip_(local_ip),
                  local_port_(local_port),
                  src_ip_(src_ip),
                  src_port_(src_port),
                  des_ip_(des_ip),
                  des_port_(des_port)
            { }

            static MsgPacket make_data_packet(const std::string& data,
                                              const std::string& src_ip, std::uint16_t src_port,
                                              const std::string& des_ip, std::uint16_t des_port) {
                MsgPacket packet(src_ip, src_port, src_ip, src_port, des_ip, des_port);
                packet.set_control("00100");
                packet.set_data(data);
                return packet;
            }

            bool feed(const char* buffer, std::uint32_t len) {
                if(len < MIN_SIZE) {
                    log_error("msg packet is invalid");
                    return false;
                }
                std::uint64_t n = 0;
                std::uint64_t m = 0;
                /* src_port_ = conv_to_ten_digits(2, buffer, (m = black_magic::bits_traits_v<decltype(src_port_)>)); */
                /* n += m; */
                /* des_port_ = conv_to_ten_digits(2, buffer + n, (m = black_magic::bits_traits_v<decltype(des_port_)>)); */
                /* n += m; */
                seq_ = conv_to_ten_digits(2, buffer + n , (m = black_magic::bits_traits_v<decltype(seq_)>));
                n += m;
                ack_ = conv_to_ten_digits(2, buffer + n , (m = black_magic::bits_traits_v<decltype(ack_)>));
                n += m;
                std::memcpy(&control_[0], buffer + n, (m = control_.size()));
                n += m;
                notice_size_ = conv_to_ten_digits(2, buffer + n, (m = black_magic::bits_traits_v<decltype(notice_size_)>));
                n += m;
                check_code_ = conv_to_ten_digits(2, buffer + n, (m = black_magic::bits_traits_v<decltype(check_code_)>));
                n += m;
                if(n != MIN_SIZE) {
                    log_error("invalid packet");
                    return false;
                }
                message_.assign(buffer + n, len - n);
                return true;
            }

            std::string to_string() const {
                std::string str;
                /* str.append(conv_to_string(2, src_port_, black_magic::bits_traits_v<decltype(src_port_)>)); */
                /* str.append(conv_to_string(2, des_port_, black_magic::bits_traits_v<decltype(des_port_)>)); */
                str.append(conv_to_string(2, seq_, black_magic::bits_traits_v<decltype(seq_)>));
                str.append(conv_to_string(2, ack_, black_magic::bits_traits_v<decltype(ack_)>));
                str.append(&control_[0], control_.size());
                str.append(conv_to_string(2, notice_size_, black_magic::bits_traits_v<decltype(notice_size_)>));
                str.append(conv_to_string(2, check_code_, black_magic::bits_traits_v<decltype(check_code_)>));
                str.append(message_);
                return str;
            }
            void print() {
                char buffer[7] = "\0";
                std::memcpy(buffer, &control_[0], control_.size());
                log_info("local ip:", local_ip_,
                         "local port:", local_port_,
                         "src ip:", src_ip(),
                         "src port:", src_port(),
                         "des ip:", des_ip(),
                         "des port:", des_port(),
                         "seq:", seq(),
                         "ack:", ack(),
                         "control:", buffer,
                         "message size:", data_size());
            }

            auto seq() const {
                return seq_;
            }
            auto ack() const {
                return ack_;
            }
            auto src_ip() const {
                return src_ip_;
            }
            auto des_ip() const {
                return des_ip_;
            }
            auto src_port() const {
                return src_port_;
            }
            auto des_port() const {
                return des_port_;
            }
            auto data_size() const {
                return message_.size();
            }
            auto data() const {
                return message_.data();
            }
            bool is_recv_ack_packet() const {
                return !is_error_packet() && local_ip_ == des_ip_ && local_port_ == des_port_ && control_[1] == '1';
            }
            bool is_recv_data_packet() const {
                return !is_error_packet() && local_ip_ == des_ip_ && local_port_ == des_port_ && control_[1] == '0';
            }
            bool is_send_ack_packet() const {
                return !is_error_packet() && (local_ip_ != des_ip_ || local_port_ != des_port_) && control_[1] == '1';
            }
            bool is_send_data_packet() const {
                return !is_error_packet() && (local_ip_ != des_ip_ || local_port_ != des_port_) && control_[1] == '0';
            }
            bool is_error_packet() const {
                return src_ip_ == des_ip_ && src_port_ == des_port_;
            }
            void swap_port() {
                std::swap(src_port_, des_port_);
            }
            void set_seq(std::uint64_t s) {
                seq_ = s;
            }
            void set_ack(std::uint64_t ack) {
                ack_ = ack;
            }
            void set_src_port(std::uint32_t port) {
                src_port_ = port;
            }
            void set_des_port(std::uint32_t port) {
                des_port_ = port;
            }
            void set_src_ip(const std::string& ip) {
                src_ip_ = ip;
            }
            void set_des_ip(const std::string& ip) {
                des_ip_ = ip;
            }
            void set_ack_flag() {
                control_[1] = '1';
            }
            void set_control(const char* buffer) {
                std::memcpy(&control_[0], buffer, control_.size());
            }
            void set_data(const std::string& data) {
                message_ = data;
            }
            void set_error_packet() {
                src_ip_ = des_ip_ = local_ip_;
                src_port_ = des_port_ = local_port_;
                std::memcpy(&control_[0], "000000", control_.size());
            }
            void clear_data() {
                message_.clear();
            }


    };
}
