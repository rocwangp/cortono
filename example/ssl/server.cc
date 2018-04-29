#include "../../cortono.hpp"
using namespace cortono::net;

int main()
{
    EventLoop base;
    TcpService service(&base, "127.0.0.1", 8080);
    service.on_message([](auto conn_ptr) {
        log_trace;
        conn_ptr->send(conn_ptr->recv_all());
    });
    service.start();
    base.loop();
    return 0;
}
