#include "module.hpp"
#include "recv_module.hpp"
#include "send_module.hpp"
#include "resend_module.hpp"
#include "lost_packet_module.hpp"

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

int main()
{
    constexpr std::uint64_t BufferSize = Power<2, 16>::value;
    constexpr std::uint64_t WindowSize = 10000;
    constexpr std::uint64_t Timeout = 200;
    constexpr std::uint64_t SendRate = 100;
    constexpr std::int32_t MaxDataSize = 4096;

    using connection_t = cortono::Connection<
                                    cortono::LostPacketModule<3, 5>,
                                    cortono::RecvModule<BufferSize, WindowSize>,
                                    cortono::SendModule<BufferSize, WindowSize>,
                                    cortono::ResendModule<BufferSize, Timeout>>;

    cortono::net::EventLoop loop;
    cortono::net::UdpService<connection_t> service(&loop, "127.0.0.1", 9999);

    std::uint64_t send_bytes = 0, recv_bytes = 0;
    std::ofstream fout{ "recv_context", std::ios_base::out };
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

    std::ifstream fin{ "send_context", std::ios_base::in };
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
        fin.read(buffer, MaxDataSize - data.size());
        data.append(buffer, fin.gcount());
        conn_ptr->run([&]{
            auto parser = cortono::ParserModule<std::uint32_t>::make_data_parser(data, "127.0.0.1", 9999, "127.0.0.1", 10000);
            if(conn_ptr->handle_packet(parser) == false) {
                if(read_file) {
                    send_buffer.append(data);
                }
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

    std::ifstream send_fin{ "send_context", std::ios_base::in };
    std::ifstream recv_fin{ "recv_context", std::ios_base::in };
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
