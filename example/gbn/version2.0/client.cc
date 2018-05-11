#include "module.hpp"
#include "recv_module.hpp"
#include "send_module.hpp"
#include "resend_module.hpp"
#include "lost_packet_module.hpp"


int main()
{
    constexpr std::uint16_t SeqBits = 16;
    constexpr std::uint64_t WindowSize = 10000;
    constexpr std::uint64_t BufferSize = black_magic::Power<2, black_magic::RoundUp<SeqBits, 8>::value>::value;

    static_assert(2 * WindowSize < BufferSize);

    constexpr std::uint64_t Timeout = 100;
    constexpr std::uint64_t SendRate = 50;
    constexpr std::int32_t MaxDataSize = 4096;

    using seq_t = black_magic::promote_t<black_magic::RoundUp<SeqBits, 8>::value>;
    using packet_t = cortono::MsgPacket<seq_t, MaxDataSize>;
    using connection_t = cortono::Connection<
                                    packet_t,
                                    cortono::LostPacketModule<1, 5>,
                                    cortono::RecvModule<BufferSize, WindowSize>,
                                    cortono::SendModule<BufferSize, WindowSize>,
                                    cortono::ResendModule<BufferSize, Timeout>>;

    cortono::net::EventLoop loop;
    cortono::net::UdpService<connection_t> service(&loop, "127.0.0.1", 9999);

    std::uint64_t send_bytes = 0, recv_bytes = 0;
    std::ofstream fout{ "recv_file.pdf", std::ios_base::out };
    service.on_read([&](std::shared_ptr<connection_t> conn_ptr) {
        std::string str = conn_ptr->get_middleware<cortono::RecvModule<BufferSize, WindowSize>>().recv_all();
        log_info(str.size());
        if(!str.empty()) {
            recv_bytes += str.size();
            fout.write(str.data(), str.size());
            if(recv_bytes > send_bytes) {
                log_fatal("error:", str.size());
            }
            log_info("....................................................");
        }
    });

    std::ifstream fin{ "send_file.pdf", std::ios_base::in };
    auto conn_ptr = service.conn_ptr();
    char buffer[MaxDataSize + 1] = "\0";
    cortono::net::Buffer send_buffer;
    std::string data;
    bool read_file = false;
    loop.run_every(std::chrono::milliseconds(SendRate),[&]{
        if(fin.eof()) {
            log_info(send_bytes, recv_bytes);
            if(send_bytes == recv_bytes) {
                loop.quit();
            }
            return;
        }
        if(!send_buffer.empty()) {
            if(send_buffer.size() > MaxDataSize) {
                data.assign(send_buffer.data(), MaxDataSize);
            }
            else {
                data.assign(send_buffer.data(), send_buffer.size());
            }
            read_file = false;
        }
        else {
            fin.read(buffer, MaxDataSize);
            data.assign(buffer, fin.gcount());
            read_file = true;
        }
        conn_ptr->run([&]{
            auto packet = packet_t::make_data_packet(data, "127.0.0.1", 9999, "127.0.0.1", 10000);
            if(conn_ptr->handle_packet(packet) == false) {
                if(read_file) {
                    send_buffer.append(data);
                }
                log_info("send packet error, store data to send buffer:", data.size());
            }
            else {
                send_bytes += data.size();
                if(!read_file) {
                    send_buffer.retrieve_read_bytes(data.size());
                }
            }
        });
        data.clear();
        log_info(send_bytes, recv_bytes);
    });
    loop.loop();

    fin.close();
    fout.close();

    std::ifstream send_fin{ "send_file.pdf", std::ios_base::in };
    std::ifstream recv_fin{ "recv_file.pdf", std::ios_base::in };
    while(!send_fin.eof() && !recv_fin.eof()) {
        if(send_fin.get() != recv_fin.get()) {
            log_fatal("error");
        }
    }
    if(send_fin.eof() && recv_fin.eof()) {
        log_info("done");
    }
    else {
        log_fatal("error");
    }
    send_fin.close();
    recv_fin.close();
    return 0;
}
