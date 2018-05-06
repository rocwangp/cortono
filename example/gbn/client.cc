#include "../../cortono.hpp"
#include "connection.hpp"

int main()
{
    cortono::net::EventLoop loop;
    cortono::net::UdpService<Connection<8, 1000>> service(&loop, "127.0.0.1", 10000);
    service.conn_ptr()->on_read([](auto conn_ptr) {
        log_info(conn_ptr->recv_all());
    });
    std::thread t([&]() {
        std::string line;
        while(true) {
            std::getline(std::cin, line);
            if(line == "exit") {
                break;
            }
            service.conn_ptr()->send(line, "127.0.0.1", 9999);
            line.clear();
        }
        loop.quit();
    });
    loop.loop();
    t.join();
    return 0;
}
