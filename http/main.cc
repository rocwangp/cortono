#include "http_server.hpp"

using namespace cortono::http;

int main() {
    HttpServer server("localhost", 9999);
    server.start();
    return 0;
}
