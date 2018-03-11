#pragma once

#include "socket.hpp"
#include "../std.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class tcp_session : public std::enable_shared_from_this<tcp_session>,
                         private util::noncopyable
    {
        public:
            tcp_session(std::shared_ptr<tcp_socket> socket)
                : socket_(socket)
            {
            }

            ~tcp_session() {}

            void on_read() { read_cb_(); }
            void on_conn() { conn_cb_(); }

            void set_read_callback(std::function<void()> cb) {
                read_cb_ = std::move(cb);
            }
            void set_conn_callback(std::function<void()> cb) {
                conn_cb_ = std::move(cb);
            }
        protected:
            std::shared_ptr<tcp_socket> socket_;

        private:
            std::function<void()> conn_cb_, read_cb_;
    };
}
