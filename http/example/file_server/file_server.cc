#include <cortono/http/app.hpp>

namespace fs = std::experimental::filesystem;

void append_fileinfo(std::string& body, const fs::directory_entry& p, int n = 0) {
    std::ostringstream oss;
    oss << p;
    std::string filename = oss.str();
    char fileimg[4096] = "\0";
    if(fs::is_directory(p)) {
        std::sprintf(fileimg, "<img src=\"img/dir.png\" width=\"24px\" height=\"24px\">");
    }
    else if(fs::is_regular_file(p)) {
        std::sprintf(fileimg, "<img src=\"img/file.png\" width=\"24px\" height=\"24px\">");
    }
    else {
        std::sprintf(fileimg, "<img src=\"img/file.png\" width=\"24px\" height=\"24px\">");
    }
    int filesize = 0;
    if(!fs::is_directory(p)) {
        filesize = fs::file_size(p);
    }

    auto ftime = fs::last_write_time(p);
    std::time_t cftime = decltype(ftime)::clock::to_time_t(ftime);
    std::string last_modify_time(std::asctime(std::localtime(&cftime)));
    char buffer[1024] = "\0";
    std::sprintf(buffer, "<p><pre>%-2d%s<a href=%s>%-15s</a>%10d %24s</pre></p>\r\n", n, std::string(fileimg).data(), filename.data(), filename.data(), filesize, last_modify_time.data());
    body.append(buffer);
}
int main()
{
    using namespace cortono;
    http::SimpleApp app;
    app.register_rule("/download/<path>")([&](const http::Request&, http::Response& res, std::string filename) {
        log_info(filename);
        if(fs::is_directory("download/" + filename)) {
            int n = 0;
            std::string body;
            for(auto& p : fs::directory_iterator("download/")) {
                append_fileinfo(body, p, n++);
            }
            log_info(body);
            res = cortono::http::Response(std::move(body));
        }
        else if(fs::is_regular_file("download/" + filename)) {
            res.send_file("download/" + filename);
            res.set_header("Content-Disposition", "attachment;filename="+filename);
        }
    });
    app.register_rule("/files")([&](const http::Request& , http::Response& res) {
        int n = 0;
        std::string body;
        for(auto& p : fs::directory_iterator("download/")) {
            append_fileinfo(body, p, n++);
        }
        log_info(body);
        res = cortono::http::Response(std::move(body));
        return;
    });
    app.register_rule("/img/<path>")([&](const http::Request& , http::Response& res, std::string filename) {
        res.send_file("img/" + filename);
        return;
    });
    app.port(9999).multithread().run();
    return 0;
}
