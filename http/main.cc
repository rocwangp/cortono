#include "app.hpp"

int main()
{
    cortono::http::SimpleApp app;
    app.register_rule("/")([]() {
        log_trace;
        return "hello world";
    });
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
#ifdef CORTONO_USE_SSL
    app.register_rule("/www/<path>")([](const cortono::http::Request&, cortono::http::Response& res, std::string filename) {
        using namespace std::experimental;
        filename = cortono::http::html_codec::decode("www/" + filename);
        log_debug(filename);
        if(filesystem::exists(filename)) {
        /* 如果filename表示一个目录，那么filesystem::file_size(filename)会阻塞
         * 标准没有规定计算目录size的情况 */
            if(filesystem::is_directory(filename)) {
                filename.append("index.html");
            }
            res = cortono::http::Response(200);
            res.read_file_to_body(filename);
        }
        else {
            res = cortono::http::Response(404);
            log_info("file is not exist");
        }
    });
#else
    app.register_rule("/www/<path>")([](const cortono::http::Request&, cortono::http::Response& res, std::string s) {
        log_info(s);
        res = cortono::http::Response(200);
        res.send_file("www/" + s);
    });
#endif

    app.register_rule("/www/<string>").methods(cortono::http::HttpMethod::POST)
        ([](const cortono::http::Request& req, std::string s) {
        if(s == "register_user_form") {
            std::unordered_map<std::string, std::string> query_kv;
            std::string query_params(std::move(req.body));
            std::size_t front = 0;
            std::size_t back = 0;
            std::string key, value;
            while(back != query_params.length()) {
                if(query_params[back] == '&') {
                    value = query_params.substr(front, back - front);
                    query_kv.emplace(std::move(key), std::move(value));
                    front = back + 1;
                    key.clear();
                    value.clear();
                }
                else if(key.empty() && query_params[back] == '=') {
                    key = query_params.substr(front, back - front);
                    front = back + 1;
                }
                ++back;
            }
            for(auto& [key, value] : query_kv) {
                log_info(key, value);
            }
        }
        return "ok";
     });
    /* app.register_rule("/www/<string>")([](const cortono::http::Request& req) { */
    /*     return "ok"; */
    /* }); */
    /* app.register_rule("/www/<path>")([](const cortono::http::Request&, cortono::http::Response& res, std::string  filename) { */
    /*     using namespace std::experimental; */
    /*     filename = cortono::http::html_codec::decode("www/" + filename); */
    /*     log_debug(filename); */
    /*     if(filesystem::exists(filename)) { */

    /*     } */
    /* }); */

    app.multithread().port(10000).run();
    return 0;
}
