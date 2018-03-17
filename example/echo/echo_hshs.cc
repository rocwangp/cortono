#include "/home/roc/unix/cortono/cortono.hpp"
using namespace cortono::net;

int main()
{
    EventLoop base;
    HSHA hsha(&base, "localhost", 9999);
    hsha.on_message([](auto c){ c->send(c->recv_all()); });
    hsha.start();
    base.loop();
    return 0;
}
