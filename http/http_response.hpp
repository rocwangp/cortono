#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_codec.hpp"

namespace cortono::http
{
    struct Response
    {
        int code{ 200 };
        bool sendfile{ false };
        std::size_t filesize{ 0 };
        std::string filename;
        std::string body;
        std::unordered_map<std::string, std::string> headers;

        Response() {}
        explicit Response(int state_code) : code(state_code) {}
        explicit Response(std::string&& context) : body(std::move(context)) {}
        Response(int state_code, std::string&& context) : code(state_code), body(std::move(context)) {}

        Response(Response&& res)
            : code(res.code),
              body(std::move(res.body)),
              headers(std::move(res.headers))
        {  }

        Response& operator=(Response&& res) {
            if(this != &res) {
                code = std::move(res.code);
                body = std::move(res.body);
                headers = std::move(res.headers);
            }
            return *this;
        }

        void set_header(std::string&& key, std::string&& value) {
            headers.emplace(std::forward<std::string>(key),
                            std::forward<std::string>(value));
        }
        void send_file(const std::string& file) {
            using namespace std::experimental;
            auto filename_decoded = html_codec::decode(file);
            log_debug(filename_decoded);
            if(filesystem::exists(filename_decoded)) {
                log_info("file exists");
                filename = filename_decoded;
                code = 200;
                sendfile = true;
                filesize = 0;
                if(filesystem::is_directory(filename)) {
                    filename.append("index.html");
                    /* for(auto& p : filesystem::recursive_directory_iterator(filename)) { */
                    /*     if(filesystem::is_regular_file(p)) { */
                    /*         filesize += filesystem::file_size(p); */
                    /*     } */
                    /* } */
                }
                /* else { */
                    filesize = filesystem::file_size(filename);
                /* } */
            }
            else {
                log_info("file is not exist");
                code = 404;
            }
        }
        bool is_send_file() const {
            return sendfile;
        }
        bool has_header(std::string&& key) const {
            return headers.count(key);
        }
        const std::string& get_header_value(std::string&& key)  {
            return headers[key];
        }
    };
}
