#pragma once

#include "adaptor.hpp"
#include "ssl_adaptor.hpp"
#include "../std.hpp"
#include "../util/noncopyable.hpp"
#include "../util/threadpool.hpp"

namespace cortono::net
{
    template <typename Connection = TcpConnection, typename Adaptor = TcpAdaptor>
    class Service : private util::noncopyable
    {
        public:
            typedef std::function<void(typename Connection::Pointer&)> ConnCallBack;
            typedef typename Connection::MessageCallBack  MessageCallBack;
            typedef typename Connection::ErrorCallBack    ErrorCallBack;
            typedef typename Connection::CloseCallBack    CloseCallBack;

            Service(EventLoop* loop, std::string_view ip, unsigned short port)
                : loop_(loop),
                  acceptor_(loop, ip, port)
            {
                acceptor_.on_connection(
                    [this]{
                        return eventloops_.empty()
                            ? loop_
                            : eventloops_[(++loop_idx_) % eventloops_.size()];
                    },
                    [this](auto&& new_conn_ptr) {
                        new_conn_ptr->on_read([this](const auto& c) {
                            if(msg_cb_) { msg_cb_(c); }
                        });
                        new_conn_ptr->on_error([this](const auto& c) {
                            if(error_cb_) { error_cb_(c); }
                        });
                        new_conn_ptr->on_close([this](const auto& c) {
                            if(close_cb_) { close_cb_(c); }
                            remove_connection(c);
                        });
                        connections_[new_conn_ptr->name()] = new_conn_ptr;
                        if(conn_cb_) {
                            conn_cb_(new_conn_ptr);
                        }
                    }
                );
            }

            ~Service() {
                for(auto loop : eventloops_) {
                    loop->quit();
                }
            }
            void on_conn(ConnCallBack cb) {
                conn_cb_ = std::move(cb);
            }
            void on_message(MessageCallBack cb) {
                msg_cb_ = std::move(cb);
            }
            void on_close(CloseCallBack cb) {
                close_cb_ = std::move(cb);
            }
            void on_error(ErrorCallBack cb) {
                error_cb_ = std::move(cb);
            }
        public:
            void start(int thread_nums = std::thread::hardware_concurrency()) {
#ifdef CORTONO_USE_SSL
                ip::tcp::ssl::init_ssl();
                ip::tcp::ssl::load_certificate(CA_CERT_FILE, SERVER_CERT_FILE, SERVER_KEY_FILE);
#endif
                while(thread_nums--) {
                    util::threadpool::instance().async([this] {
                        EventLoop loop;
                        {
                            std::unique_lock lock{ mutex_ };
                            eventloops_.emplace_back(&loop);
                        }
                        loop.loop();
                    });
                }
                util::threadpool::instance().start();
                acceptor_.start();
            }
        private:
            void remove_connection(const typename Connection::Pointer& conn) {
                std::unique_lock lock { mutex_ };
                connections_.erase(conn->name());
            }
        private:
            EventLoop *loop_ = nullptr;
            Adaptor acceptor_;
            int loop_idx_ = -1;
            std::mutex mutex_;
            std::vector<EventLoop*> eventloops_;
            std::unordered_map<std::string, typename Connection::Pointer> connections_;
            ConnCallBack conn_cb_ = nullptr;
            MessageCallBack msg_cb_ = nullptr;
            ErrorCallBack error_cb_ = nullptr;
            CloseCallBack close_cb_ = nullptr;
    };

    using TcpService = Service<>;
#ifdef CORTONO_USE_SSL
    using SslService = Service<SslConnection, SslAdaptor>;
#endif
}
