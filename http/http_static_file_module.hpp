#include "../cortono.hpp"
#include "http_module.hpp"
#include "mime_type.hpp"

#include <fstream>
#include <experimental/filesystem>

namespace cortono::http
{
    using namespace std::experimental;
    using namespace cortono::util;

    class http_static_file_module : public http_module
    {
        public:
            http_static_file_module() : http_module() {}
            virtual ~http_static_file_module() {}
            virtual module_handle_status handle(http_request& req, http_response& res,
                                                std::shared_ptr<cort_socket> socket) override {
                log_trace;
                if(auto method = req.method(); method != GET ) {
                    log_error("method error");
                    return module_handle_status::done;
                }

                auto uri = req.uri();
                if(uri.back() == '/') {
                    log_error("dir error");
                    return module_handle_status::done;
                }

                if(uri.find("..") != std::string_view::npos) {
                    return module_handle_status::error;
                }

                if(auto pos = uri.find_first_not_of('/');
                        pos != std::string_view::npos) {
                    uri = uri.substr(pos);
                }
                filesystem::path p { uri_to_path(uri) };
                log_debug(p);
                if(!file_exist(p)) {
                    log_error("file not exist");
                    return module_handle_status::error;
                }
                if(filesystem::is_symlink(p)) {
                    log_error("file is symbol link");
                    return module_handle_status::error;
                }

                res.set_status_and_content(status_type::ok);

                auto file_size = filesystem::file_size(p);
                res.set_header(response_header::content_length, file_size);

                auto ftime = filesystem::last_write_time(p);
                std::time_t cftime = decltype(ftime)::clock::to_time_t(ftime);
                std::string file_time = std::asctime(std::localtime(&cftime));
                file_time.pop_back();
                res.set_header(response_header::last_modified_time, file_time);

                res.set_header(response_header::content_type, mime_type(uri));

                log_trace;
                socket->write(res.response_header());
                exitif(socket->write_file(uri, file_size) == -1, std::strerror(errno));
                return module_handle_status::done;
            }

            std::string_view name() const {
                return name_;
            }

        private:
            filesystem::path uri_to_path(std::string_view uri) {
                return filesystem::absolute(uri);
            }

            bool file_exist(filesystem::path& p) {
                return filesystem::exists(p);
            }

            std::string_view mime_type(std::string_view uri) {
                std::size_t dot_index = uri.find_last_of('.');
                if(dot_index == std::string_view::npos) {
                    return {};
                }
                else {
                    return get_mime_type(uri.substr(dot_index));
                }
            }

        private:
            std::string_view name_ = "static_file"sv;
    };
};
