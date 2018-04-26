#include "app.h"

int main()
{
    cortono::http::SimpleApp app;
    app.register_rule("/")([]() { return "hello world"; });
    app.register_rule("/adder/<int>/<int>")([](int a, int b) {
        std::stringstream oss;
        oss << a + b;
        return oss.str();
    });
    app.register_rule("/adder/<int>/<int>/<int>")([](int a, int b, int c) {
        std::stringstream oss;
        oss << a + b + c;
        return oss.str();
    });
    app.register_rule("/echo/<string>")([](std::string s) {
        return s;
    });
    app.register_rule("/info")([](const cortono::http::Request& req) {
        std::stringstream oss;
        oss << req.method_to_string() << " "
            << req.raw_url << " "
            << "HTTP/" << req.version.first << "." << req.version.second
            << "\r\n";
        for(auto& [key, value] : req.header_kv_pairs) {
            oss << key << ": " << value << "\r\n";
        }
        oss << "\r\n";
        oss << req.body;
        return oss.str();
    });
    app.multithread().run();
    return 0;
}
