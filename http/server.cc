#include "app.h"

int main()
{
    using namespace cortono::http;
    SimpleApp app;
    app.register_rule("/")([](const Request& req) {
        std::stringstream oss;
        oss << req.method_to_string() << " "
            << req.raw_url << " "
            << "HTTP/" << req.version.first << "." << req.version.second;
        for(auto& [key, value] : req.header_kv_pairs) {
            oss << key << ": " << value << "\r\n";
        }
        oss << "\r\n";
        oss << req.body;
        log_debug(oss.str());
        return "hello world";
    });
    app.bindaddr("127.0.0.1")
       .port(10000)
       .multithread()
       .run();
    return 0;
}
