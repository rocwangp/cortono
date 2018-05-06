#include "../../cortono.hpp"
#include "connection.hpp"

int main()
{
    cortono::net::EventLoop loop;
    cortono::net::UdpService<Connection<32, 1000>> service(&loop, "127.0.0.1", 10000);
    std::size_t cnt = 0;
    loop.run_every(std::chrono::milliseconds(100), [&] {
        if(cnt < 100) {
            service.conn_ptr()->send_message("127.0.0.1", 9999, "hello world");
            ++cnt;
        }
        else {
            if(service.conn_ptr()->is_done()) {
                loop.quit();
            }
        }
    });
    loop.loop();
    return 0;
}
