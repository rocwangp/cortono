#include "http_server.hpp"
#include "http_static_file_module.hpp"
#include "http_index_module.hpp"

using namespace cortono::http;

int main() {
    http_server server("localhost", 9999);
    server.register_module(std::make_shared<http_index_module>());
    server.register_module(std::make_shared<http_static_file_module>());
    server.start();
    return 0;
}
