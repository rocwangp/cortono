#include "../../cortono.hpp"
int main()
{
    cortono::net::EventLoop base;
    base.runAfter(std::chrono::milliseconds(1000), []{
        log_info("after timer expires");
    });
    base.runEvery(std::chrono::milliseconds(2000), []{
        log_info("every timer expires");
    });
    base.loop();
    return 0;
}
