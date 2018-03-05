#include "../cortono.h"

#include "http_router.h"
#include "http_session.h"
#include <regex>
#include <map>
#include <iostream>

namespace cortono::http
{
    using namespace cortono::net;

    class http_session;
    class request;
    class response;

    template <typename session_type = http_session,
              typename service_policy = cort_service<session_type>,
              typename eventloop_policy = cort_eventloop>
    class http_server_t : private util::noncopyable
    {
        public:
            using handler_type = std::function<void(std::stringstream&, request&)>;
            using resource_type = std::map<std::string, std::unordered_map<std::string, handler_type>>;

        public:
            http_server_t(std::string_view ip, unsigned short port)
                : service_(&base_, ip, port)
            {
                init_conn_callback();
            }

            void start() {
                service_.start();
                base_.sync_loop();
            }

            template <http_method... Methods, typename Function>
            void set_http_handler(std::string_view source, Function&& f) {
                http_router_.register_handler<Methods...>(source, std::forward<Function>(f));
            }

        private:
            void init_conn_callback() {
                http_handler_ = [this](const request& req, response& res){
                    bool success = http_router_.route(req.get_method(), req.get_url(), req, res);
                    if(!success){
                        res.set_status_and_content(status_type::bad_request, "the url is not right");
                    }
                };

                service_.on_conn([this](auto socket){
                    log_trace;
                    socket->enable_option(cort_socket::NO_DELAY);
                    service_.register_session(socket, std::make_shared<session_type>(socket, http_handler_));
                });
            }
        private:
            eventloop_policy base_;
            service_policy service_;

            http_router http_router_;
            http_handler http_handler_ = nullptr;
    };

    using http_server = http_server_t<>;
}
