#include "module.hpp"
#include "recv_module.hpp"
#include "send_module.hpp"
#include "resend_module.hpp"
#include "lost_packet_module.hpp"
#include "modify_module.hpp"


int main()
{
    constexpr std::uint16_t SeqBits = 16;
    constexpr std::uint64_t WindowSize = 10000;
    constexpr std::uint64_t BufferSize = black_magic::Power<2, black_magic::RoundUp<SeqBits, 8>::value>::value;

    static_assert(2 * WindowSize < BufferSize);

    constexpr std::uint64_t Timeout = 100;
    constexpr std::int32_t MaxDataSize = 4096;

    using seq_t = black_magic::promote_t<black_magic::RoundUp<SeqBits, 8>::value>;
    using packet_t = cortono::MsgPacket<seq_t, MaxDataSize>;
    using connection_t = cortono::Connection<
                                    packet_t,
                                    cortono::LostPacketModule<3, 5>,
                                    cortono::RecvModule<BufferSize, WindowSize>,
                                    cortono::SendModule<BufferSize, WindowSize>,
                                    cortono::ResendModule<BufferSize, Timeout>,
                                    cortono::LostPacketModule<3, 5>,
                                    cortono::ModifyModule>;

    cortono::net::EventLoop loop;
    cortono::net::UdpService<connection_t> service(&loop, "127.0.0.1", 10000);
    cortono::net::Buffer send_buffer;
    service.on_read([&](std::shared_ptr<connection_t> conn_ptr) {
        std::string str = conn_ptr->get_middleware<cortono::RecvModule<BufferSize, WindowSize>>().recv_all();
        if(!str.empty()) {
            send_buffer.append(str);
        }
        if(send_buffer.empty()) {
            return;
        }
        if(send_buffer.size() > MaxDataSize) {
            str.assign(send_buffer.data(), MaxDataSize);
        }
        else {
            str.assign(send_buffer.data(), send_buffer.size());
        }
        log_info(str.size());
        auto [ip, port] = conn_ptr->get_middleware<cortono::RecvModule<BufferSize, WindowSize>>().peer_ip_port();
        auto packet = conn_ptr->make_data_packet(str, ip, port);
        if(conn_ptr->handle_packet(packet)) {
            send_buffer.retrieve_read_bytes(str.size());
        }
        log_info(".............................................");
        /* exit(0); */
    });
    loop.loop();

    return 0;
}

