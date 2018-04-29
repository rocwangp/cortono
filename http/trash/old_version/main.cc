#include "http_server.hpp"

using namespace cortono::http;

int main() {
    ::signal(SIGPIPE, SIG_IGN);
    HttpServer server("localhost", 9999);
    server.start();
    return 0;
}
