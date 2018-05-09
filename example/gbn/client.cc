#include "../../cortono.hpp"
#include "connection.hpp"

int main()
{
    cortono::net::EventLoop loop;
    cortono::net::UdpService<Connection<16, 200, 4000>> service(&loop, "127.0.0.1", 10000);
    std::ifstream fin{ "send_context", std::ios_base::in };
    std::ofstream fout{ "recv_context", std::ios_base::out };
    std::uint64_t send_bytes = 0;
    std::uint64_t recv_bytes = 0;
    service.conn_ptr()->on_read([&](auto conn_ptr) {
        auto str = conn_ptr->recv_all();
        fout.write(str.data(), str.size());
        recv_bytes += str.size();
    });
    loop.run_every(std::chrono::milliseconds(100), [&, conn_ptr = service.conn_ptr()] {
        if(fin.eof() && conn_ptr->is_done()) {
            log_info(send_bytes, recv_bytes);
            if(send_bytes == recv_bytes) {
                loop.quit();
            }
            /* loop.quit(); */
        }
        else if(!fin.eof() && conn_ptr->can_send()) {
            char buffer[1024] = "\0";
            fin.read(buffer, sizeof(buffer) - 1);
            auto n = fin.gcount();
            if((std::size_t)n < sizeof(buffer) - 1) {
                log_info(".........................", n,"....................");
            }
            send_bytes += n;
            std::string line(buffer, n);
            conn_ptr->send(line, "127.0.0.1", 9999);
        }
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
    send_fin.close();
    recv_fin.close();
    return 0;
}
