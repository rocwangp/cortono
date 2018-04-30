#include "http_client.hpp"

int main()
{
    using namespace cortono;
    net::EventLoop base;
    auto conn_ptr = net::SslClient::connect(&base, "www.baidu.com", 443);
    conn_ptr->send("GET / HTTP/1.1\r\n\r\n");
    conn_ptr->on_read([](auto& conn_ptr) {
        log_info(conn_ptr->recv_all());
    });
    base.loop();
    return 0;
}
