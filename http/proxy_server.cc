#include "app.h"

void sigabrt_handler(int) {
    log_trace;
}

int main()
{
    ::signal(SIGABRT, sigabrt_handler);
    using namespace cortono::http;
    SimpleApp app;
    app.bindaddr("127.0.0.1")
       .port(9999)
       .proxy_server()
       /* .https() */
       .multithread()
       .run();
    return 0;
}
