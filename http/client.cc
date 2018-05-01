#include "http_client.hpp"

int main()
{
    using namespace cortono;
    net::EventLoop base;
    auto conn_ptr = net::SslClient::connect(&base, "www.baidu.com", 443, [](auto conn_ptr) {
        log_info(conn_ptr->recv_all());
    });
    if(conn_ptr) {
        conn_ptr->send("GET / HTTP/1.1\r\nHost: www.baidu.com:443\r\nConnection: keep-alive\r\n\r\n");
        base.loop();
    }
    return 0;
}
