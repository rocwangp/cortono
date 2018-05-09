#pragma once

#include "../../../cortono.hpp"
#include <array>

namespace cortono
{
    template <typename T>
    struct bits_traits {};

#define INTERNAL_BITS_MAPPING(T, N) \
    template <> \
    struct bits_traits<T> \
    {\
        static const std::uint16_t value = N; \
    };


    INTERNAL_BITS_MAPPING(std::uint16_t, 16);
    INTERNAL_BITS_MAPPING(std::uint32_t, 32);
    INTERNAL_BITS_MAPPING(std::uint64_t, 64);

    template <typename T>
    static constexpr auto bits_traits_v = bits_traits<T>::value;


    std::uint64_t conv_to_ten_digits(std::uint16_t s, const char* buffer, std::uint32_t len) {
        std::uint64_t res = 0;
        std::uint64_t base = 1;
        for(int i = len - 1; i >= 0; --i) {
            res += base * (buffer[i] - '0');
            base *= s;
        }
        return res;
    }
    std::string conv_to_string(std::uint16_t d, std::uint64_t n, std::uint32_t len) {
        std::string str;
        for(std::uint32_t i = 0; i != len; ++i) {
            str.append(1, '0' + n % d);
            n /= d;
        }
        std::reverse(str.begin(), str.end());
        return str;
    }

    template <typename T>
    struct MsgPacket
    {
        MsgPacket()  { message.clear(); }
        std::uint16_t src_port{ 0 };
        std::uint16_t des_port{ 0 };
        T seq_num{ 0 };
        T ack_num{ 0 };
        std::array<char, 6> control = { {'0', '0', '0', '0', '0', '0' } };
        std::uint16_t notice_size{ 0 };
        std::uint16_t check_code{ 0 };

        std::string message;

        enum {
            MIN_SIZE =
                bits_traits_v<decltype(src_port)> +
                bits_traits_v<decltype(des_port)> +
                bits_traits_v<decltype(seq_num)> +
                bits_traits_v<decltype(ack_num)> +
                6 +
                bits_traits_v<decltype(notice_size)> +
                bits_traits_v<decltype(check_code)>,

            MAX_SIZE = MIN_SIZE + 4096
        };

        /* std::string to_string() const { */
        /*     std::string str; */
        /*     str.append(conv_to_string<2>(src_port, bits_traits_v<decltype(src_port)>)); */
        /*     str.append(conv_to_string<2>(des_port, bits_traits_v<decltype(des_port)>)); */
        /*     str.append(conv_to_string<2>(seq_num, bits_traits_v<decltype(seq_num)>)); */
        /*     str.append(conv_to_string<2>(ack_num, bits_traits_v<decltype(ack_num)>)); */
        /*     str.append(&control[0], control.size()); */
        /*     str.append(conv_to_string<2>(notice_size, bits_traits_v<decltype(notice_size)>)); */
        /*     str.append(conv_to_string<2>(check_code, bits_traits_v<decltype(check_code)>)); */
        /*     str.append(message); */
        /*     return str; */
        /* } */
    };

    template <typename T>
    class ParserModule
    {
        public:
            using self_t = ParserModule<T>;
            static self_t make_data_parser(const std::string& data,
                                           const std::string& src_ip, std::uint16_t src_port,
                                           const std::string& des_ip, std::uint16_t des_port) {
                (void)(des_ip);
                self_t parser(src_ip, src_port);
                parser.packet_.src_port = src_port;
                parser.packet_.des_port = des_port;
                std::memcpy(&parser.packet_.control[0], "00100", parser.packet_.control.size());
                parser.packet_.message = data;
                return parser;
            }

            ParserModule(const std::string& ip, const std::uint16_t& port)
                : ip_(ip),
                  port_(port)
            { }

            bool feed(const char* buffer, std::uint32_t len) {
                if(len < MsgPacket<T>::MIN_SIZE) {
                    log_error("msg packet is invalid");
                    return false;
                }
                std::uint64_t n = 0;
                std::uint64_t m = 0;
                packet_.src_port = conv_to_ten_digits(2, buffer, (m = bits_traits_v<decltype(packet_.src_port)>, m));
                n += m;
                packet_.des_port = conv_to_ten_digits(2, buffer + n, (m = bits_traits_v<decltype(packet_.des_port)>,  m));
                n += m;
                packet_.seq_num = conv_to_ten_digits(2, buffer + n , (m = bits_traits_v<decltype(packet_.seq_num)>,  m));
                n += m;
                packet_.ack_num = conv_to_ten_digits(2, buffer + n , (m = bits_traits_v<decltype(packet_.ack_num)>,  m));
                n += m;
                std::memcpy(&packet_.control[0], buffer + n, (m = packet_.control.size(),  m));
                n += m;
                packet_.notice_size = conv_to_ten_digits(2, buffer + n, (m = bits_traits_v<decltype(packet_.notice_size)>,  m));
                n += m;
                packet_.check_code = conv_to_ten_digits(2, buffer + n, (m = bits_traits_v<decltype(packet_.check_code)>,  m));
                n += m;
                n = MsgPacket<T>::MIN_SIZE;
                packet_.message.assign(buffer + n, len - n);

                print_packet();
                return true;
            }
            void print_packet() {
                log_info("src port:", src_port(),
                         "des port:", des_port(),
                         "seq:", seq(),
                         "ack:", ack(),
                         "message:", packet_.message);
            }
            std::string to_string() const {
                log_trace;
                std::string str;
                str.append(conv_to_string(2, packet_.src_port, bits_traits_v<decltype(packet_.src_port)>));
                str.append(conv_to_string(2, packet_.des_port, bits_traits_v<decltype(packet_.des_port)>));
                str.append(conv_to_string(2, packet_.seq_num, bits_traits_v<decltype(packet_.seq_num)>));
                str.append(conv_to_string(2, packet_.ack_num, bits_traits_v<decltype(packet_.ack_num)>));
                str.append(&packet_.control[0], packet_.control.size());
                str.append(conv_to_string(2, packet_.notice_size, bits_traits_v<decltype(packet_.notice_size)>));
                str.append(conv_to_string(2, packet_.check_code, bits_traits_v<decltype(packet_.check_code)>));
                str.append(packet_.message);
                return str;
            }
            MsgPacket<T> to_packet() {
                return packet_;
            }
            auto seq() const {
                return packet_.seq_num;
            }
            auto ack() const {
                return packet_.ack_num;
            }
            auto src_port() const {
                return packet_.src_port;
            }
            auto des_port() const {
                return packet_.des_port;
            }
            auto data_size() const {
                return packet_.message.size();
            }
            auto data() const {
                return packet_.message.data();
            }
            bool is_recv_ack_packet() const {
                return port_ == packet_.des_port && packet_.control[1] == '1';
            }
            bool is_recv_data_packet() const {
                return port_ == packet_.des_port && packet_.control[1] == '0';
            }
            bool is_send_ack_packet() const {
                return port_ != packet_.des_port && packet_.control[1] == '1';
            }
            bool is_send_data_packet() const {
                return port_ != packet_.des_port && packet_.control[1] == '0';
            }
            void swap_port() {
                std::swap(packet_.src_port, packet_.des_port);
            }
            void set_seq(std::uint64_t s) {
                packet_.seq_num = s;
            }
            void set_ack(std::uint64_t ack) {
                packet_.ack_num = ack;
            }
            void set_src_port(std::uint32_t port) {
                packet_.src_port = port;
            }
            void set_des_port(std::uint32_t port) {
                packet_.des_port = port;
            }
            void set_ack_flag() {
                packet_.control[1] = '1';
            }
            void clear_data() {
                packet_.message.clear();
            }
        private:
            MsgPacket<T> packet_;

            std::string ip_;
            std::uint16_t port_;
    };
}
