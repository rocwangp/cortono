#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_codec.hpp"
#include "http_session_manager.hpp"

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
                sendfile = std::move(res.sendfile);
                filesize = std::move(res.filesize);
                filename = std::move(res.filename);
                body = std::move(res.body);
                headers = std::move(res.headers);
            }
            return *this;
        }

        void set_header(std::string&& key, std::string&& value) {
            headers[std::forward<std::string>(key)] = std::forward<std::string>(value);
        }
        void set_domain(std::string&& domain) {
            domain_ = std::move(domain);
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
                }
                filesize = filesystem::file_size(filename);
            }
            else {
                log_info("file is not exist");
                code = 404;
            }
        }
        void read_file_to_body(const std::string& file) {
            using namespace std::experimental;
            filesize = filesystem::file_size(file);
            body.resize(filesize);
            std::ifstream fin{ file, std::ios_base::in };
            fin.read(&body[0], filesize);
            fin.close();
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
        std::shared_ptr<Session> start_session(const std::string& domain) {
            static const std::string CORTONO_SESSIONID = "SESSIONID";
            session_ = session_manager::create_session(domain_, domain);
            set_header("Set-Cookie", session_->get_cookie().to_string());
            return session_;
        }
        std::shared_ptr<Session> start_session() {
            static const std::string CORTONO_SESSIONID = "SESSIONID";
            session_ = session_manager::create_session(domain_, CORTONO_SESSIONID);
            set_header("Set-Cookie", session_->get_cookie().to_string());
            return session_;
        }
        bool has_cookie() {
            return false;
        }
    private:
        std::string domain_;
        std::shared_ptr<Session> session_;
    };
}
