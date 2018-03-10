#include "http_server.h"
#include <fstream>
using namespace cortono::http;

int main() {
    http_server server("localhost", 9999);

    server.set_http_handler<GET, POST>("/", [](const request& , response& res) {
        log_trace;
        res.set_status_and_content(status_type::ok, "hello world");
    });

    server.set_http_handler<HEAD>("/index.html", [](const request&, response& res){
        std::ifstream fin("index.html", std::ios_base::in);
        fin.seekg(0, std::ios_base::end);
        auto file_size = fin.tellg();
        res.set_status_and_content(status_type::ok);
        res.add_header("Content-Length", std::to_string(file_size));
    });

    server.set_http_handler<OPTIONS>("*", [](const request&, response& res){
        res.add_header("Allow", "GET POST, PUT, OPTIONS, HEAD");
        res.set_status_and_content(status_type::ok);
    });

    server.set_http_handler<GET, POST>("/test", [](const request& req, response& res){
        auto name = req.get_header_value("name");
        if(name.empty()){
            res.set_status_and_content(status_type::bad_request, "no name");
            return;
        }

        auto id = req.get_query_value("id");
        if(id.empty()) {
            res.set_status_and_content(status_type::bad_request);
            return;
        }

        res.set_status_and_content(status_type::ok, "hello world");
    });

    server.start();
    return 0;
}
