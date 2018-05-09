#pragma once

#include "../../cortono.hpp"
#include <random>

static bool probability_random(int mole, int deno) {
    if(rand() % 60000 < 60000 * mole / deno) {
        return true;
    }
    else {
        return false;
    }
}
static std::string ten_to_binary_string(std::uint64_t m, std::int16_t bits) {
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

std::uint64_t binary_to_ten(const char* first, int len) {
    std::uint64_t res = 0;
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

INTERNAL_TYPE_MAPPING(8, std::uint16_t);
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

template <std::uint64_t N, std::uint64_t M>
struct Power
{
    static const std::uint64_t value = N * Power<N, M - 1>::value;
};

template <std::uint64_t N>
struct Power<N, 0>
{
    static const std::uint64_t value = 1;
};


template <std::uint64_t BufferSize, std::uint64_t WindowSize, typename T>
class SlideWindow
{
    public:
        void move(std::uint64_t steps) {
            for(std::uint64_t i = 0; i != steps; ++i) {
                win_.reset((left_ + i) % BufferSize) ;
            }
            left_ = (left_ + steps) % BufferSize ;
            right_ = (right_ + steps) % BufferSize;
        }
        void move_to(std::uint64_t left) {
            while(left_ != left) {
                win_.reset(left_);
                left_ = (left_ + 1) % BufferSize;
                right_ = (right_ + 1) % BufferSize;
            }
        }
        void set_flag(std::uint64_t pos, std::uint64_t len) {
            for(std::uint64_t i = 0; i != len; ++i) {
                win_.set((pos + i) % BufferSize) ;
            }
            try_to_move();
        }
        void set_data(std::uint64_t pos, const char* data, std::uint64_t len) {
            for(std::uint64_t i = 0; i != len; ++i) {
                win_.set((pos + i) % BufferSize);
                buffer_[(pos + i) % BufferSize] = data[i];
            }
            try_to_move();
        }
        std::string read_all() {
            return read_bytes(readable_bytes_);
        }
        std::string read_bytes(std::uint64_t n) {
            n = std::min(n, readable_bytes_);
            std::string info(n, '\0');
            for(std::uint64_t i = 0; i != n; ++i) {
                info[i] = buffer_[(read_idx_ + i) % BufferSize];
            }
            if(BufferSize - read_idx_ <= n) {
                read_idx_ = n - (BufferSize - read_idx_);
            }
            else {
                read_idx_ += n;
            }
            readable_bytes_ -= n;
            return info;
        }
        std::uint64_t left_bound() const {
            return left_;
        }
        std::uint64_t right_bound() const {
            return right_;
        }
        std::uint64_t win_size() const {
            return WindowSize;
        }
        std::uint64_t buffer_size() const {
            return BufferSize;
        }
        std::uint64_t readable_size() const {
            return readable_bytes_;
        }
        std::uint64_t is_full() const {
            return readable_bytes_ == BufferSize;
        }
    private:
        void try_to_move() {
            while(win_.test(left_) && (readable_bytes_ == 0 || left_ != read_idx_)) {
                win_.reset(left_);
                left_ = (left_ + 1) % BufferSize;
                right_ = (right_ + 1) % BufferSize;
                ++readable_bytes_;
            }
        }
    private:
        std::uint64_t left_{ 0 };
        std::uint64_t right_{ WindowSize };
        std::bitset<BufferSize> win_;

        std::uint64_t read_idx_{ 0 };
        std::uint64_t readable_bytes_{ 0 };
        std::array<char, BufferSize> buffer_;
};


template <int Bits,
          int Time,
          std::uint64_t SlideWinSize = Power<2, Bits - 2>::value - 1,
          std::uint64_t MsgSize = SlideWinSize,
          int SeqBits = RoundUp<Bits, 8>::value,
          typename SeqType = promote_t<SeqBits>>
class Connection : public cortono::net::UdpConnection,
                   public std::enable_shared_from_this<
                                Connection<Bits, Time, SlideWinSize, MsgSize, SeqBits, SeqType>>
{
    public:
        template <int Size>
        struct Message
        {
            std::uint16_t src_port;
            std::uint16_t des_port;
            SeqType seq_num;
            SeqType ack_num;
            char control[6];
            std::string data;

            std::string to_string() const {
                std::string str;
                str.append(ten_to_binary_string(src_port, 16));
                str.append(ten_to_binary_string(des_port, 16));
                str.append(ten_to_binary_string(seq_num, SeqBits));
                str.append(ten_to_binary_string(ack_num, SeqBits));
                str.append(control, 6);
                str.append(data);
                return str;
            }
            static constexpr std::uint64_t HEADER_SIZE = 2 * 16 + 2 * SeqBits + 6;
            static constexpr std::uint64_t MAX_DATA_SIZE = Size;
            static constexpr std::uint64_t MAX_MSG_SIZE = HEADER_SIZE + MAX_DATA_SIZE;
        };


        using parent_t = cortono::net::UdpConnection;
        using slide_window_t = SlideWindow<Power<2, SeqBits>::value, SlideWinSize, SeqType>;
        using message_t = Message<MsgSize>;
        using sequence_t = SeqType;
        using read_callback_t = std::function<void(std::shared_ptr<Connection>)>;

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
        void on_read(read_callback_t cb) {
            read_cb_ = std::move(cb);
        }
        void handle_read() {
            std::string ip;
            char buffer[message_t::MAX_MSG_SIZE + 1] = "\0";
            int bytes = this->parent_t::recv(buffer, sizeof(buffer) - 1, ip, client_port_);
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
        bool is_done() {
            return send_buffer_->empty() && timers_.empty();
        }
        bool can_send() const {
            return send_buffer_->size() < 1024;
        }
        void send(const std::string& msg, const std::string& ip, std::uint16_t port) {
            client_ip_ = ip;
            client_port_ = port;
            send_buffer_->append(msg);
            send_data_message();
        }
        std::string recv_all() {
            return recv_windows_.read_all();
        }
    private:
        bool is_ack_cross_border(SeqType ack_num) {
            auto send_left = send_windows_.left_bound();
            auto send_right = send_windows_.right_bound();
            if(send_left < send_right && ack_num <= send_left) {
                return true;
            }
            else if(send_left < send_right && ack_num > send_right) {
                return true;
            }
            else if(send_left > send_right && ack_num <= send_left && ack_num > send_right) {
                return true;
            }
            else {
                return false;
            }
        }
        void handle_ack_message(message_t&& msg) {
            if(is_ack_cross_border(msg.ack_num)) {
                log_info("recv ack:", msg.ack_num,
                         "is cross border",
                         "send_windows_left:", send_windows_.left_bound(),
                         "send_windows_right:", send_windows_.right_bound());
            }
            else {
                log_info("recv ack:", msg.ack_num);
                while(!timers_.empty()) {
                    auto start_num = std::get<0>(timers_.front());
                    auto end_num = std::get<1>(timers_.front());
                    auto ack_num = msg.ack_num;
                    if(msg.ack_num == start_num) {
                        break;
                    }
                    log_info("cancel timer:", start_num, "to", end_num);
                    loop_->cancel_timer(std::get<2>(timers_.front()));
                    timers_.pop_front();
                }
                log_info("send_windows move to", msg.ack_num);
                send_windows_.move_to(msg.ack_num);
                send_data_message();
            }
        }
        bool has_idle_serial_num() {
            auto left_bound = send_windows_.left_bound();
            auto right_bound = send_windows_.right_bound();
            if(left_bound < right_bound) {
                if(seq_ >= right_bound) {
                    return false;
                }
            }
            else {
                if(seq_ < left_bound && seq_ >= right_bound) {
                    return false;
                }
            }
            return true;
        }
        void send_data_message() {
            if(send_buffer_->empty()) {
                log_info("send buffer is empty, return");
                return;
            }
            if(!has_idle_serial_num()) {
                log_info("no serial_num available",
                         "cur serial_num:", seq_,
                         "send_windows left bound:", send_windows_.left_bound(),
                         "send_windows right bound:", send_windows_.buffer_size());
                return;
            }
            auto msg = make_data_message();
            auto data_size = msg.data.size();
            auto str = msg.to_string();
            auto it = total_res_map_.emplace(seq_, std::pair{ (seq_ + data_size) % send_windows_.buffer_size(), 1 });

            auto bytes = this->parent_t::send(str.data(), str.size(), client_ip_, client_port_);
            log_info("send new message with serial_num:", seq_, "to", (seq_ + data_size) % send_windows_.buffer_size());
            /* log_info(bytes, str.size(), str); */

            auto start_seq = seq_;
            auto end_seq = start_seq;
            if(send_windows_.buffer_size() - start_seq >= data_size) {
                end_seq = start_seq + data_size;
            }
            else {
                end_seq = data_size - (send_windows_.buffer_size() - start_seq);
            }
            auto timer_id = loop_->run_every(
                std::chrono::milliseconds(timeout_),
                [=] {
                    log_info(client_port_, "resend message:", start_seq, "to", end_seq);
                    ++it->second.second;
                    this->parent_t::send(str.data(), str.size(), "127.0.0.1", client_port_);
                }
            );
            timers_.emplace_back(start_seq, end_seq - 1, timer_id);
            if(send_windows_.buffer_size() - seq_ < data_size) {
                seq_ = data_size - (send_windows_.buffer_size() - seq_);
            }
            else {
                seq_ += data_size;
            }
        }
        bool has_idle_room_to_recv(std::uint64_t serial_num) {
            auto left_bound = recv_windows_.left_bound();
            auto right_bound = recv_windows_.right_bound();
            if(left_bound < right_bound) {
                if(serial_num >= right_bound) {
                    return false;
                }
            }
            else {
                if(serial_num < left_bound && serial_num >= right_bound) {
                    return false;
                }
            }
            return true;
        }
        void handle_data_message(message_t&& msg) {
            if(recv_windows_.is_full()) {
                log_info("recv windows is full, throw the message");
            }
            else if(!has_idle_room_to_recv(msg.seq_num)) {
                log_info("no room available to recv data",
                         "data seq:", msg.seq_num,
                         "recv_windows left_bound:", recv_windows_.left_bound(),
                         "recv_windows right bound:", recv_windows_.right_bound());
            }
            else {
                if(probability_random(4, 5)) {
                    log_info("suppose the ack is lost");
                }
                else {
                    if(msg.seq_num < recv_windows_.left_bound()) {
                        log_info("recv message but the seq_num:", msg.seq_num, "is less than recv_windows left bound:", recv_windows_.left_bound(), "ignore it");
                    }
                    else {
                        log_info("recv message and set flag in recv_windows with seq_num:", msg.seq_num, "to", msg.seq_num + msg.data.size());
                        recv_windows_.set_data(msg.seq_num, msg.data.data(), msg.data.size());
                    }
                    send_ack_message();

                    if(read_cb_) {
                        read_cb_(this->shared_from_this());
                    }
                    else {
                        recv_windows_.read_all();
                    }
                }
            }
        }
        void send_ack_message() {
            message_t msg = make_ack_message();
            std::string str = msg.to_string();
            int bytes = this->parent_t::send(str.data(), str.size(), "127.0.0.1", client_port_);
            if(bytes <= 0) {
                log_fatal("send error", std::strerror(errno));
            }
            log_info("start send ack message with ack:", msg.ack_num);
        }
        message_t make_data_message() {
            message_t msg;
            msg.src_port = server_port_;
            msg.des_port = client_port_;
            msg.seq_num = seq_;
            msg.ack_num = 0;
            std::memcpy(msg.control, "001000", sizeof(msg.control));
            if(send_buffer_->size() + seq_ <= send_windows_.right_bound()) {
                msg.data = send_buffer_->read_all();
            }
            else {
                std::uint64_t n = 0;
                if(send_windows_.left_bound() > send_windows_.right_bound()) {
                    if(seq_ >= send_windows_.left_bound()) {
                        n = send_windows_.buffer_size() - seq_ + send_windows_.right_bound();
                    }
                    else {
                        n = send_windows_.right_bound() - seq_;
                    }
                }
                else {
                    n = send_windows_.right_bound() - seq_;
                }
                n = std::min(n, message_t::MAX_DATA_SIZE);
                msg.data = send_buffer_->read_bytes(n);
            }
            /* log_info(msg.data.size()); */
            /* print_message(msg); */
            return msg;
        }
        message_t make_ack_message() {
            message_t msg;
            msg.src_port = server_port_;
            msg.des_port = client_port_;
            msg.seq_num = seq_;
            msg.ack_num = recv_windows_.left_bound();
            std::memcpy(msg.control, "011000", sizeof(msg.control));
            msg.data.clear();
            /* print_message(msg); */
            return msg;
        }
        message_t parse_message(const char* buffer, int len) {
            message_t msg;
            msg.src_port = binary_to_ten(buffer, 16);
            msg.des_port = binary_to_ten(buffer + 16, 16);
            msg.seq_num = binary_to_ten(buffer + 32, SeqBits);
            msg.ack_num = binary_to_ten(buffer + 32 + SeqBits, SeqBits);
            std::memcpy(msg.control, buffer + 32 + 2 * SeqBits, 6);
            msg.data.assign(buffer + 32 + 2 * SeqBits + 6, len - 32 - 2 * SeqBits - 6);
            /* print_message(msg); */
            return msg;
        }
        void print_message(message_t msg) {
            log_info("\nsrc port:", msg.src_port, "\n",
                     "des port:", msg.des_port, "\n",
                     "seq_num:", msg.seq_num, "\n",
                     "ack num:", msg.ack_num, "\n"
                     "control:", std::string(msg.control, 6), "\n",
                     "data:", msg.data, msg.data.size(),
                     "done");
        }
    private:
        cortono::net::EventLoop* loop_;
        std::string server_ip_;
        std::uint16_t server_port_;
        std::string client_ip_;
        std::uint16_t client_port_;
        std::shared_ptr<cortono::net::Buffer> recv_buffer_, send_buffer_;

        sequence_t seq_{ 0 };
        slide_window_t recv_windows_, send_windows_;
        read_callback_t read_cb_{ nullptr };

        std::uint64_t timeout_{ Time };
        std::list<std::tuple<sequence_t, sequence_t, cortono::net::Timer::timer_id>> timers_;
        std::multimap<sequence_t, std::pair<sequence_t, std::uint32_t>> total_res_map_;

    private:
        static_assert(Power<2, SeqBits>::value > 2 * SlideWinSize, "SlideWinSize is too large or the SerialNumBits is too small");
};

