#include "app.h"

int main()
{
    using namespace cortono::http;
    SimpleApp app;
    app.bindaddr("127.0.0.1")
       .port(9999)
       .multithread()
       .proxy_server()
       .run();
    return 0;
}
