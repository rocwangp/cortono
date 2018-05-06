#include "../../cortono.hpp"
#include "connection.hpp"

int main()
{
    cortono::net::EventLoop loop;
    cortono::net::UdpService<Connection<32, 4000>> service(&loop, "127.0.0.1", 9999);
    loop.loop();
    return 0;
}
