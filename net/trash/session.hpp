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
            tcp_session(std::shared_ptr<TcpSocket> socket)
                : socket_(socket)
            {
            }

            ~tcp_session() {}

            void on_read() {
                log_trace;
                if(!read_cb_)
                  log_debug("read_cb is nullptr");
                else
                  read_cb_();
            }
            void on_conn() { if(conn_cb_) conn_cb_(); }

            void set_read_callback(std::function<void()> cb) {
                read_cb_ = std::move(cb);
            }
            void set_conn_callback(std::function<void()> cb) {
                conn_cb_ = std::move(cb);
            }
        protected:
            std::shared_ptr<TcpSocket> socket_;

        private:
            std::function<void()> conn_cb_, read_cb_;
    };
}
