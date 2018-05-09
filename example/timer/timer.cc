#include "../../cortono.hpp"
int main()
{
    cortono::net::EventLoop base;
    base.run_after(std::chrono::milliseconds(1000), []{
        log_info("after timer expires");
    });
    base.run_every(std::chrono::milliseconds(2000), []{
        log_info("every timer expires");
    });
    base.loop();
    return 0;
}
