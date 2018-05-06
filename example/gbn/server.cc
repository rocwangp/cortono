#include "../../cortono.hpp"
#include "connection.hpp"

int main()
{
    cortono::net::EventLoop loop;
    cortono::net::UdpService<Connection<8, 1000>> service(&loop, "127.0.0.1", 9999);
    service.conn_ptr()->on_read([](auto conn_ptr) {
        conn_ptr->send(conn_ptr->recv_all(), "127.0.0.1", 10000);
    });
    loop.loop();
    return 0;
}
