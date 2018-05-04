#include "../../cortono.hpp"

int main()
{
    using namespace cortono::net;
    EventLoop base;
    base.runAfter(std::chrono::seconds(1), [&base]{ base.quit(); });
    base.loop();
    return 0;
}
