#include "../../cortono.hpp"
using namespace cortono::net;
int main() {
    EventLoop base;
    TcpService service(&base, "127.0.0.1", 9999);
    service.on_message([](auto conn) { conn->send(conn->recv_all()); });
    service.start(8);
    // base.run_after(std::chrono::seconds(5), [&service]{ service.stop(); });
    base.loop();
    return 0;
}
