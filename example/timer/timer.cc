#include "../../cortono.hpp"
int main()
{
    cortono::net::EventLoop base;
    base.runAfter(std::chrono::milliseconds(1000), []{
        log_info("timer expires");
    });
    base.loop();
    return 0;
}
