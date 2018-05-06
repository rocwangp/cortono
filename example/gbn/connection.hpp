#pragma once

#include "../../cortono.hpp"
#include <random>
template <int N>
struct TwoPower
{
    static const std::uint64_t value = 2 * TwoPower<N - 1>::value;
};

template <>
struct TwoPower<0>
{
    static const std::uint64_t value = 1;
};


static bool probability_random(int mole, int deno) {
    if(rand() % 60000 < 60000 * mole / deno) {
        return true;
    }
    else {
        return false;
    }
}
static std::string ten_to_binary_string(std::uint16_t m, std::int16_t bits) {
    std::string str;
    while(bits > 0) {
        int n = m % 2;
        str.append(1, '0' + n);
        m /= 2;
        --bits;
    }
    std::reverse(str.begin(), str.end());
    return str;
}

template <typename T>
T binary_to_ten(const char* first, int len) {
    T res = 0;
    int n = 1;
    for(int i = len - 1; i >= 0; --i) {
        if(*(first + i) == '1') {
            res += n;
        }
        n *= 2;
    }
    return res;
}

template <int Bytes>
struct promote { };

#define INTERNAL_TYPE_MAPPING(bytes, t) \
    template <> \
    struct promote<bytes> \
    { \
        using type = t; \
    };

INTERNAL_TYPE_MAPPING(8, std::uint8_t);
INTERNAL_TYPE_MAPPING(16, std::uint16_t);
INTERNAL_TYPE_MAPPING(32, std::uint32_t);
INTERNAL_TYPE_MAPPING(64, std::uint64_t);

template <int Bytes>
using promote_t = typename promote<Bytes>::type;


template <int N, int M = 8, int MAX = 64>
struct RoundUp
{
    static const int value = N >= MAX ? MAX : ((N + (M - 1)) & (~(M - 1)));
};

template <int N, int M>
struct Power
{
    static const std::uint64_t value = N * Power<N, M - 1>::value;
    /* static const std::uint64_t value = std::conditional<M == 0, 1, N * Power<N, M - 1>::value; */
};

template <int N>
struct Power<N, 0>
{
    static const std::uint64_t value = 1;
};


template <std::uint64_t BufferSize, std::uint64_t WindowSize>
class SlideWindow
{
    public:
        void move(std::uint64_t steps) {
            for(std::uint64_t i = 0; i != steps; ++i) {
                win_.reset((left_ + i) % BufferSize);
            }
            left_ = (left_ + steps) % BufferSize;
            right_ = (right_ + steps) % BufferSize;
        }
        void move_to(std::uint64_t left) {
            for(auto i = left_; i != left; i = (i + 1) % BufferSize) {
                win_.reset(i);
            }
            left_ = left;
            if((left_ + WindowSize) % BufferSize < right_) {
                right_ = WindowSize - (BufferSize - left_);
            }
            else {
                right_ = left_ + WindowSize;
            }
        }
        void set_flag(std::uint64_t pos, std::size_t len) {
            for(std::size_t i = 0; i != len; ++i) {
                win_.set((pos + i) % BufferSize);
            }
            try_to_move();
        }
        void set_data(std::uint64_t pos, const char* data, std::size_t len) {
            for(std::size_t i = 0; i != len; ++i) {
                win_.set((pos + i) % BufferSize);
                buffer_[pos + i] = data[i];
            }
        }
        std::size_t readable() const {
            if(left_ > read_idx_) {
                return left_ - read_idx_;
            }
            else {
                return BufferSize - read_idx_ + left_;
            }
        }
        std::string read_all() {
            return read_bytes(readable());
        }
        std::string read_bytes(std::size_t n) {
            n = std::min(n, readable());
            std::string info(n, '\0');
            for(std::size_t i = 0; i != n; ++i) {
                info[i] = buffer_[(read_idx_ + i) % BufferSize];
            }
            read_idx_ = (read_idx_ + n) % BufferSize;
            return info;
        }
        auto left_bound() const {
            return left_;
        }
        auto right_bound() const {
            return right_;
        }
        std::uint64_t size() const {
            return WindowSize;
        }
    private:
        void try_to_move() {
            while(win_.test(left_)) {
                win_.reset(left_);
                left_ = (left_ + 1) % BufferSize;
                right_ = (right_ + 1) % BufferSize;
            }
        }
    private:
        std::uint64_t left_{ 0 };
        std::uint64_t right_{ WindowSize };
        std::bitset<BufferSize> win_;

        std::uint64_t read_idx_{ 0 };
        std::array<char, BufferSize> buffer_;
};


template <int Bits,
          int Time,
          int SlideWinSize = 50,
          int MsgSize = 1024,
          int SerialNumBits = RoundUp<Bits, 8>::value,
          typename SerialNumType = promote_t<SerialNumBits>>
class Connection : public cortono::net::UdpConnection
{
    public:
        template <int Size>
        struct Message
        {
            std::uint16_t src_port;
            std::uint16_t des_port;
            SerialNumType seq_num;
            SerialNumType ack_num;
            char control[6];
            /* std::uint16_t control : 6; */
            std::string data;

            std::string to_string() const {
                std::string str;
                str.append(ten_to_binary_string(src_port, 16));
                str.append(ten_to_binary_string(des_port, 16));
                str.append(ten_to_binary_string(seq_num, SerialNumBits));
                str.append(ten_to_binary_string(ack_num, SerialNumBits));
                str.append(control, 6);
                str.append(data);
                return str;
            }
            static const int MIN_LEN = 16 * 2 + SerialNumBits * 2 + 6;
            static const int MAX_LEN = Size;
        };


        using slide_window_t = SlideWindow<Power<2, SerialNumBits>::value, SlideWinSize>;
        using message_t = Message<MsgSize>;

        Connection(cortono::net::EventLoop* loop, int fd, const std::string& server_ip, std::uint16_t server_port)
            : UdpConnection(fd),
              loop_(loop),
              server_ip_(server_ip),
              server_port_(server_port),
              recv_buffer_(std::make_shared<cortono::net::Buffer>()),
              send_buffer_(std::make_shared<cortono::net::Buffer>())
        {
            std::srand((unsigned short)(time(nullptr)));
        }

        ~Connection() {
            for(auto& [serial_start, msg_pair] : total_res_map_) {
                log_info("[", serial_start, ":", msg_pair.first, "]:", msg_pair.second);
            }
        }
        void handle_read() {
            std::string ip;
            std::uint16_t port;
            char buffer[1024] = "\0";
            int bytes = this->recv(buffer, sizeof(buffer), ip, port);
            if(bytes > 0) {
                auto msg = parse_message(buffer, bytes);
                if(msg.data.empty() && msg.control[1] == '1') {
                    handle_ack_message(std::move(msg));
                }
                else {
                    handle_data_message(std::move(msg));
                }
            }
            log_info("............................................................");
        }
        void handle_write() {

        }
         void handle_close() {

        }
        void send_message(const std::string& ip, std::uint16_t port, const std::string& msg) {
            client_ip_ = ip;
            client_port_ = port;
            send_buffer_->append(msg);
            send_data_message();
        }

        bool is_done() {
            return send_buffer_->empty() && timers_.empty();
        }
    private:
        void handle_ack_message(message_t&& msg) {
            if(msg.ack_num < send_windows_.left_bound()) {
                log_info("recv ack:", msg.ack_num, "is less than send_windows left bound", "ignore it");
                return;
            }
            /* else if(msg.ack_num > send_windows_.right_bound()) { */
            /*     log_info("recv ack:", msg.ack_num, "is more than send_windows right bound", "ignore it"); */
            /*     return; */
            /* } */
            else {
                log_info("recv ack:", msg.ack_num);
                while(!timers_.empty() && timers_.front().first < msg.ack_num) {
                    log_info("cancel timer:", timers_.front().first);
                    loop_->cancel_timer(timers_.front().second);
                    timers_.pop_front();
                }
                log_info("send_windows move to", msg.ack_num);
                send_windows_.move_to(msg.ack_num);
                send_data_message();
            }
        }
        void send_data_message() {
            if(send_buffer_->empty()) {
                return;
            }
            if(serial_num_ >= send_windows_.right_bound()) {
                log_info("serial_num", serial_num_, "is more than send windows right bound:", send_windows_.left_bound(), ", return");
                return;
            }
            auto msg = make_data_message();
            auto str = msg.to_string();
            /* log_info(str, msg.data.size()); */
            total_res_map_.emplace(serial_num_, std::pair{ serial_num_ + msg.data.size(), 1 });
            this->send(str.data(), str.size(), client_ip_, client_port_);
            auto data_size = msg.data.size();
            log_info("send new message with serial_num:", serial_num_, "to", serial_num_ + data_size);
            auto timer_id = loop_->run_every(
                std::chrono::milliseconds(timeout_),
                [serial_num = serial_num_, data_size = data_size, str = std::move(str), this] {
                    log_info("resend message:", serial_num, "to", serial_num + data_size);
                    ++total_res_map_[serial_num].second;
                    this->send(str.data(), str.size(), client_ip_, client_port_);
                }
            );
            timers_.emplace_back(serial_num_, timer_id);
            if(serial_num_ + data_size < serial_num_) {
                serial_num_ = data_size - (send_windows_.size() - serial_num_);
            }
            else {
                serial_num_ += data_size;
            }
        }
        void handle_data_message(message_t&& msg) {
            if(recv_windows_.left_bound() < recv_windows_.right_bound() && msg.seq_num > recv_windows_.right_bound()) {
                log_info("recv message but the seq_num:", msg.seq_num, "is more than recv_windows right bound, ignore it");
                return;
            }
            else {
                if(probability_random(1, 5)) {
                    log_info("suppose the ack is lost");
                }
                else {
                    if(msg.seq_num >= recv_windows_.left_bound()) {
                        log_info("recv message and set flag in recv_windows with seq_num:", msg.seq_num, "to", msg.seq_num + msg.data.size());
                        recv_windows_.set_data(msg.seq_num, msg.data.data(), msg.data.size());
                    }
                    else {
                        log_info("recv message but the seq_num:", msg.seq_num, "is less than recv_windows right bound, ignore it");
                    }
                    send_ack_message();
                }
            }
        }
        void send_ack_message() {
            message_t msg = make_ack_message();
            std::string str = msg.to_string();
            log_info(client_ip_, client_port_);
            int bytes = this->send(str.data(), str.size(), "127.0.0.1", 10000);
            if(bytes <= 0) {
                log_fatal("send error", std::strerror(errno));
            }
            log_info("start send ack message with ack:", msg.ack_num);
        }
        message_t make_data_message() {
            message_t msg;
            msg.src_port = server_port_;
            msg.des_port = client_port_;
            msg.seq_num = serial_num_;
            msg.ack_num = 0;
            std::memcpy(msg.control, "001000", sizeof(msg.control));
            if(send_buffer_->size() <= message_t::MAX_LEN - message_t::MIN_LEN) {
                msg.data = send_buffer_->read_all();
            }
            else {
                msg.data = send_buffer_->read_bytes(message_t::MAX_LEN - message_t::MIN_LEN);
            }
            return msg;
        }
        message_t make_ack_message() {
            message_t msg;
            msg.src_port = server_port_;
            msg.des_port = client_port_;
            msg.seq_num = serial_num_;
            msg.ack_num = recv_windows_.left_bound();
            std::memcpy(msg.control, "011000", sizeof(msg.control));
            msg.data.clear();
            return msg;
        }
        message_t parse_message(const char* buffer, int len) {
            message_t msg;
            msg.src_port = binary_to_ten<std::uint16_t>(buffer, 16);
            msg.des_port = binary_to_ten<std::uint16_t>(buffer + 16, 16);
            msg.seq_num = binary_to_ten<std::uint32_t>(buffer + 32, SerialNumBits);
            msg.ack_num = binary_to_ten<std::uint32_t>(buffer + 32 + SerialNumBits, SerialNumBits);
            std::memcpy(msg.control, buffer + 32 + 2 * SerialNumBits, 6);
            msg.data.assign(buffer + 32 + 2 * SerialNumBits + 6, len - message_t::MIN_LEN);
            return msg;
        }
        void print_message(message_t msg) {
            log_info("\nsrc port:", msg.src_port, "\n",
                     "des port:", msg.des_port, "\n",
                     "seq_num:", msg.ack_num, "\n",
                     "ack num:", msg.ack_num, "\n"
                     "control:", std::string(msg.control, 6), "\n",
                     "data:", msg.data, msg.data.size());
        }
    private:
        cortono::net::EventLoop* loop_;
        std::string server_ip_;
        std::uint16_t server_port_;
        std::string client_ip_;
        std::uint16_t client_port_;
        std::uint64_t serial_num_;
        std::shared_ptr<cortono::net::Buffer> recv_buffer_, send_buffer_;
        slide_window_t recv_windows_, send_windows_;

        std::uint64_t timeout_{ Time };
        std::list<std::pair<std::uint64_t, cortono::net::Timer::timer_id>> timers_;

        std::map<std::uint64_t, std::pair<std::uint32_t, std::uint32_t>> total_res_map_;

    private:
        static_assert(MsgSize > Message<MsgSize>::MIN_LEN, "MsgSize is too small");
        static_assert(Power<2, SerialNumBits>::value > 2 * SlideWinSize, "Buffer is too large or the SerialNumBits is too small");
};

