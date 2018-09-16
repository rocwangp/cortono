#pragma once

#include "../std.hpp"
#include "../util/noncopyable.hpp"
#include "../util/threadpool.hpp"
#include "adaptor.hpp"
#include "ssl_adaptor.hpp"

namespace cortono::net
{
    // 兼容TCP和SSL
    // 对于TCP，使用TcpConnection和TcpAdaptor
    // 对于SSL，使用SslConnection和SslAdaptor
    template <typename Connection = TcpConnection, typename Adaptor = TcpAdaptor>
    class Service : private util::noncopyable
    {
        public:
            // FIXME: const Connection::Pointer&
            typedef std::function<void(typename Connection::Pointer)> ConnCallBack;
            typedef typename Connection::MessageCallBack  MessageCallBack;
            typedef typename Connection::ErrorCallBack    ErrorCallBack;
            typedef typename Connection::CloseCallBack    CloseCallBack;

            Service(EventLoop* loop, std::string_view ip, unsigned short port)
                : loop_(loop),
                  acceptor_(loop, ip, port)
            {
                // 由于Connection类型不确定，只有Adaptor内部知道如何创建Connection对象
                // 所以代替将参数传给Service，改为在Adaptor内部构造后返回给Service
                acceptor_.on_connection(
                    // Adaptor需要知道选择哪个EventLoop
                    [this]{
                        return eventloops_.empty()
                            ? loop_
                            : eventloops_[(++loop_idx_) % eventloops_.size()];
                    },
                    // 建立连接后的回调，这里传入的是Connection::Pointer而非构造Connection的参数
                    [this](auto&& new_conn_ptr) { 
                        // called in new_conn_ptr->loop() thread
                        new_conn_ptr->set_conn_state(Connection::ConnState::Connected);
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
                        {
                            std::unique_lock lock{ mutex_ };
                            connections_[new_conn_ptr->name()] = new_conn_ptr;
                        }
                        if(conn_cb_) {
                            conn_cb_(new_conn_ptr);
                        }
                    }
                );
            }

            ~Service() {
                if(!is_quit_) {
                    stop();
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
            EventLoop* acquire_eventloop() {
                std::unique_lock lock{ mutex_ };
                return eventloops_.size() ? eventloops_[(++loop_idx_) % eventloops_.size()]
                                          : loop_;
            }
        public:
            void start(int thread_nums = std::thread::hardware_concurrency()) {
                start_threadpool(thread_nums);
                start_acceptor();
            }
            void start_threadpool(int thread_nums = std::thread::hardware_concurrency()) {
#ifdef CORTONO_USE_SSL
                ip::tcp::ssl::init_ssl();
                ip::tcp::ssl::load_certificate(CA_CERT_FILE, SERVER_CERT_FILE, SERVER_KEY_FILE);
#endif
                for(int i = 0; i < thread_nums; ++i) {
                    // FIXME: thread_nums应为线程池的大小，应该传给instance函数
                    util::threadpool::instance().async([this] {
                        EventLoop loop;
                        {
                            std::unique_lock lock{ mutex_ };
                            eventloops_.emplace_back(&loop);
                        }
                        loop.loop();
                    });
                }
                util::threadpool::instance().start(thread_nums);
            }
            void start_acceptor() {
                acceptor_.start();
            }
            void stop() {
                for(auto it = connections_.begin(); it != connections_.end();) {
                    (it++)->second->close();
                }
                log_info("connection close done, start quit eventloops");
                for(auto& loop : eventloops_) {
                    loop->quit();
                }
                log_info("eventloops quit done, start close threadpool");
                util::threadpool::instance().quit();
                log_info("threadpool close done, start quit main loop");
                loop_->quit();
                log_info("main loop quit done, service quit done");

                is_quit_ = true;
            }
        private:
            void remove_connection(const typename Connection::Pointer& conn) {
                std::unique_lock lock { mutex_ };
                connections_.erase(conn->name());
            }
        private:
            EventLoop *loop_{ nullptr };
            Adaptor acceptor_;
            int loop_idx_{ -1 };
            std::mutex mutex_;
            std::vector<EventLoop*> eventloops_;
            std::unordered_map<std::string, typename Connection::Pointer> connections_;
            ConnCallBack conn_cb_{ nullptr };
            MessageCallBack msg_cb_{ nullptr };
            ErrorCallBack error_cb_{ nullptr };
            CloseCallBack close_cb_{ nullptr };

            bool is_quit_{ false };
    };

    using TcpService = Service<>;
#ifdef CORTONO_USE_SSL
    using SslService = Service<SslConnection, SslAdaptor>;
#endif
}
