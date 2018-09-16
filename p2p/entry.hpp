#pragma once

#include "../cortono.hpp"
#include "unility.hpp"

namespace p2p {

class BitPacker {
public:
    using self_t = BitPacker;

    template <typename T>
    self_t& pack_number(T n, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0) {
        context_.append(number_to_string<T>(n));
        return *this;
    }
    template <std::size_t N>
    self_t& pack_char_array(const std::array<char, N>& chars) {
        std::copy(chars.begin(), chars.end(), std::back_inserter(context_));
        return *this;
    }
    self_t& pack_string(const std::string& context) {
        context_.append(context);
        return *this;
    }
    std::string to_string() const {
        return context_;
    }
private:
    std::string context_;
};

class BitParser {
public:
    using self_t = BitParser;
    BitParser() : index_(0) {}
    BitParser(std::string_view context) : index_(0), context_(context) {}

    template <typename T>
    self_t& parse_number(T& n, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0) {
        using type = std::remove_reference_t<T>;
        n = string_to_number<type>(context_.data() + index_);
        index_ += sizeof(type);
        return *this;
    }
    template <std::size_t N>
    self_t& parse_char_array(std::array<char, N>& chars) {
        for(std::size_t i = 0; i != N; ++i) {
            chars[i] = context_[index_ + i];
        }
        index_ += N;
        return *this;
    }
    template <std::size_t N>
    self_t& parse_dotted_address(std::array<char, N>& chars) {
        std::size_t index{ 0 };
        char* str_end = nullptr;
        const char* str_begin = context_.data();
        unsigned long n = std::strtoul(str_begin, &str_end, 10);
        while(str_begin != str_end && index != N) {
            chars[index++] = static_cast<char>(n);
            str_begin = str_end + 1;
            n = std::strtoul(str_begin, &str_end, 10);
        }
        return *this;
    }
    template <std::size_t N>
    self_t& parse_readable_address(const std::array<char, N>& chars, std::string& ip) {
        ip.clear();
        for(std::size_t i = 0; i != N; ++i) {
            ip.append(std::to_string(static_cast<int>(chars[i])));
            if(i + 1 != N) {
                ip.append(1, '.');
            }
        }
        return *this;
    }
private:
    std::size_t index_{ 0 };
    std::string_view context_;
};

class Datagram {
public:
    enum Command {
        PING,
        PONG,
        ADDR,
        GETADDR,
        UNKNOWN
    };
    struct DatagramHeader {
        friend std::stringstream& operator<<(std::stringstream& oss, const DatagramHeader& datagram);

        Command command{ Command::UNKNOWN };
        std::array<char, 4> ip{ '\0', '\0', '\0', '\0' };
        std::uint16_t port{ 0 };
        std::uint16_t checksum{ 0 };
        std::uint32_t body_size{ 0 };
    };
    static constexpr std::size_t HEADER_LENGTH = 13;
    static constexpr std::uint64_t MAX_MESSAGE_SIZE = 2 * 1024 * 1024;

    Datagram() {}
    Datagram(DatagramHeader&& header) : header_(std::move(header)) {}
    Datagram(std::string&& ip, std::uint16_t port) {
        BitParser{ ip }.parse_dotted_address(header_.ip);
        header_.port = port;
    }
    Datagram(const std::string& ip, std::uint16_t port) {
        BitParser{ ip }.parse_dotted_address(header_.ip);
        header_.port = port;
    }
    ~Datagram() {}

    static DatagramHeader parse_datagram_header(std::string_view entry) {
        DatagramHeader header;

        std::string ip;
        std::uint8_t command = 0;
        BitParser { entry }
              .parse_number(command)
              .parse_char_array(header.ip)
              .parse_number(header.port)
              .parse_number(header.checksum)
              .parse_number(header.body_size)
              .parse_readable_address(header.ip, ip);
        header.command = Datagram::Command(command);

        return header;
    }
    void parse_datagram_body(const std::string& entry) {
        body_ = entry.substr(0, header_.body_size);
    }

    std::string serialize() const {
        std::string results = BitPacker{} 
            .pack_number(static_cast<std::uint8_t>(header_.command))
            .pack_char_array(header_.ip)
            .pack_number(header_.port)
            .pack_number(header_.checksum)
            .pack_number(static_cast<std::uint32_t>(body_.size()))
            .pack_string(body_)
            .to_string();
        return results;
    }
    std::size_t body_size() const {
        return header_.body_size;
    }
    std::string ip() const {
        std::string res;
        BitParser{}.parse_readable_address(header_.ip, res);
        return res;
    }
    std::uint16_t port() const {
        return header_.port;
    }
    std::pair<std::string, std::uint16_t> endpoint() const {
        return { ip(), port() };
    }
    Command type() const {
        return header_.command;
    }
protected:
    DatagramHeader header_;
    std::string body_{ "hello p2p" };
};

class AddrDatagram : public Datagram {
public:
    AddrDatagram() { header_.command = AddrDatagram::type; }
    AddrDatagram(Datagram::DatagramHeader&& header) : Datagram(std::move(header)) {}
    AddrDatagram(const std::string& ip, std::uint16_t port) : Datagram(ip, port) { header_.command = AddrDatagram::type; }
    static constexpr Datagram::Command type = Datagram::Command::ADDR;
};

class GetAddrDatagram : public Datagram {
public:
    GetAddrDatagram() { header_.command = GetAddrDatagram::type; }
    GetAddrDatagram(Datagram::DatagramHeader&& header) : Datagram(std::move(header)) {}
    GetAddrDatagram(const std::string& ip, std::uint16_t port) : Datagram(ip, port) { header_.command = GetAddrDatagram::type; }
    static constexpr Datagram::Command type = Datagram::Command::GETADDR;
};

class PingDatagram : public Datagram {
public:
    PingDatagram() { header_.command = PingDatagram::type; }
    PingDatagram(Datagram::DatagramHeader&& header) : Datagram(std::move(header)) {}
    static constexpr Datagram::Command type = Datagram::Command::PING;
};

class PongDatagram : public Datagram {
public:
    PongDatagram() { header_.command = PongDatagram::type; }
    PongDatagram(Datagram::DatagramHeader&& header) : Datagram(std::move(header)) {}
    static constexpr Datagram::Command type = Datagram::Command::PONG;
};

inline std::ostringstream& operator<<(std::ostringstream& oss, const Datagram::DatagramHeader& header) {
    std::string ip;
    BitParser{}.parse_readable_address(header.ip, ip);
    oss << "\n"
        << "command: " << header.command << "\n"
        << "ip: " << ip << "\n"
        << "port: " << header.port << "\n"
        << "checksum: " << header.checksum << "\n"
        << "body_size: " << header.body_size;
    return oss;
}

inline std::string_view type_to_name(const Datagram& datagram) {
    using namespace std::literals;
    switch(datagram.type()) 
    {
        case AddrDatagram::type:
            return "AddrDatagram"sv;
        case GetAddrDatagram::type:
            return "GetAddrDatagram"sv;
        case PingDatagram::type:
            return "PingDatagram"sv;
        case PongDatagram::type:
            return "PongDatagram"sv;
        default:
            return "UnknownDatagram"sv;
    }
}


} // namespace p2p
