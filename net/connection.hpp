#pragma once

#include "../std.hpp"
#include "../util/noncopyable.hpp"
#include "socket.hpp"
#include "ssl_socket.hpp"
#include "eventloop.hpp"

namespace cortono::net
{
    /*
     * 考虑到TCP和SSL仅在io api上有少量差异，所以可以采用同个Connection代表连接
     * 模板参数传入TcpSocket或SslSocket，其中SslSocket继承自TcpSocket
     * 二者具有相同接口，保证了Connection实现的统一性
     */
        template <typename Socket>
        class Connection : public std::enable_shared_from_this<Connection<Socket>>
        {
            public:
                /*
                 * 对于非阻塞connect api，有以下三种情况
                 * 1.connect立即成功，返回0，对应Connected状态
                 * 2.connect返回-1，errno为EINPROGRESS，对应HandShaking握手状态
                 * 3.connect返回-1，连接失败
                 *
                 * 此外Closed状态也用于避免二次关闭的情况，如以下情况
                 * 1.在一次io回调中close掉当前连接，随后没有返回poller而是执行另一个io回调
                 *   可以通过判断状态来决定是否有必要执行io操作
                 * 2.使用代理服务器的情况下，
                 *   client与proxy的连接关闭会导致proxy与server的连接关闭
                 *   proxy与server的连接关闭会导致client与proxy的连接关闭
                 *   通过状态判断可以反之二次关闭
                 */
                enum class ConnState
                {
                    HandShaking,
                    Connected,
                    Closed
                };
                using socket_t = Socket;

                typedef std::shared_ptr<Connection<Socket>>             Pointer;
                /* 由于回调传入的是shared_from_this()，是右值，所以类型不能是左值引用 */
                /* FIXME: 改成const Connection::Pointer& */
                typedef std::function<void(Connection::Pointer)> MessageCallBack;
                typedef Connection::MessageCallBack                     CloseCallBack;
                typedef Connection::MessageCallBack                     ErrorCallBack;

                 // TcpSocket的构造函数接收EventLoop*和fd
                 // SslSocket的构造函数接收EventLoop*，fd和SSL*
                template <typename... Args>
                Connection(EventLoop* loop, Args... args)
                    : loop_(loop),
                      socket_(args...),
                      recv_buffer_(std::make_shared<Buffer>()),
                      send_buffer_(std::make_shared<Buffer>())
                {
                    name_ = std::move(socket_.local_address() + ":" + socket_.peer_address());
                    socket_.tie(loop_->poller());
                    socket_.set_option(socket_t::non_block);
                    socket_.set_read_callback(std::bind(&Connection::handle_read, this));
                    socket_.set_close_callback(std::bind(&Connection::handle_close, this));
                    socket_.set_write_callback(std::bind(&Connection::handle_write, this));
                    socket_.enable_reading();
                    // 由于采用边缘触发，即使打开可读监听也不会无限调用可读回调
                    socket_.enable_writing();
                }

                bool is_closed() {
                    return conn_state_ == ConnState::Closed;
                }
                void set_conn_state(ConnState state) {
                    conn_state_ = state;
                }
                ConnState conn_state() const {
                    return conn_state_;
                }
                // TcpSocket提升到SslSocket，用处不大
                void reset_to_ssl() {
                    if(std::is_same_v<Socket, TcpSocket>) {
                        socket_.reset_to_ssl_socket();
                    }
                }
                void close() {
                    // 防止二次关闭
                    if(conn_state_ != ConnState::Closed) {
                        handle_close();
                    }
                }
                void enable_reading() {
                    socket_.enable_reading();
                }
                void enable_writing() {
                    socket_.enable_writing();
                }
                void disable_reading() {
                    socket_.disable_reading();
                }
                void disable_writing() {
                    socket_.disable_writing();
                }
                void disable_all() {
                    socket_.disable_all();
                }
                void on_read(MessageCallBack cb) {
                    read_cb_ = std::move(cb);
                }
                void on_write(MessageCallBack cb) {
                    write_cb_ = std::move(cb);
                }
                void on_close(CloseCallBack cb) {
                    close_cb_ = std::move(cb);
                }
                void on_error(ErrorCallBack cb) {
                    error_cb_ = std::move(cb);
                }
                std::string name() {
                    return name_;
                }
                EventLoop* loop() {
                    return loop_;
                }
                std::string recv_all() {
                    return recv_buffer_->read_all();
                }
                std::string recv_util(std::string_view s) {
                    return recv_buffer_->read_util(s);
                }
                std::string_view recv_string_view() {
                    return recv_buffer_->read_string_view();
                }
                auto recv_buffer() {
                    return recv_buffer_;
                }
                void clear_recv_buffer() {
                    recv_buffer_->clear();
                }
                void send(const char* buffer, int len) {
                    if(len == 0) {
                        return;
                    }
                    // 如果正处于握手状态（客户端），则将数据添加到缓冲区等待连接建立后再发送
                    if(!send_buffer_->empty() || conn_state_ == ConnState::HandShaking) {
                        send_buffer_->append(buffer, len);
                        return;
                    }
                    auto bytes = socket_.send(buffer, len);
                    if(bytes == 0) {
                        log_info("send return 0, close connection...");
                        handle_close();
                    }
                    else if(bytes == -1) {
                        if(errno == EINTR || errno == EAGAIN) {
                            log_info("send is -1 and errno is EINTR | EAGAIN, call send again...");
                            //FIXME: call send again ?
                            /* send(msg); */
                        }
                    }
                    else if(bytes != static_cast<int>(len)) {
                        //没发完，设回调
                        socket_.set_write_callback(std::bind(&Connection::handle_write, this));
                        send_buffer_->append(buffer + bytes, len - bytes);
                    }

                }
                void send(const std::string& msg) {
                    send(msg.data(), msg.size());
                    /* if(msg.empty()) { */
                    /*     return; */
                    /* } */
                    /* if(!send_buffer_->empty() || conn_state_ == ConnState::HandShaking) { */
                    /*     log_trace("handshaking, store data to send_buffer"); */
                    /*     send_buffer_->append(msg); */
                    /*     return; */
                    /* } */
                    /* auto bytes = socket_.send(msg.data(), msg.size()); */
                    /* if(bytes == 0) { */
                    /*     log_info("send return 0, close connection..."); */
                    /*     handle_close(); */
                    /* } */
                    /* else if(bytes == -1) { */
                    /*     /1* if(errno == EINTR || errno == EAGAIN) { *1/ */
                    /*         /1* log_info("send is -1 and errno is EINTR | EAGAIN, call send again..."); *1/ */
                    /*         /1* /2* send(msg); *2/ *1/ */
                    /*     /1* } *1/ */
                    /* } */
                    /* else if(bytes != static_cast<int>(msg.size())) { */
                    /*     socket_.set_write_callback(std::bind(&Connection::handle_write, this)); */
                    /*     send_buffer_->append(msg.substr(bytes)); */
                    /* } */
                }
                void sendfile(const std::string& filename) {
                    if(filename.empty()) {
                        return;
                    }
                    fileoffet_ = 0;
                    filesize_ = util::get_filesize(filename);
                    if(filesize_ == 0) {
                        return;
                    }
                    sendfile_ = true;
                    filename_ = filename;
                    if(!send_buffer_->empty()) {
                        return;
                    }
                    handle_sendfile();
                }

            private:
                // connect没有立即成功后需要等待套接字可读并可写, 再通过getsockopt方可判断连接建立成功
                // 对于TcpSocket，仅仅检查fd是否可写
                // 对于SslSocket，还需要执行SSL_connect
                // FIXME: 对于SslSocket而言，fd的connect是非阻塞的，ssl的connect是阻塞的
                void handle_handshake() {
                    if(socket_.handshake()) {
                        log_info("handshake done");
                        conn_state_ = ConnState::Connected;
                    }
                    else {
                        log_info("handshake error");
                        handle_close();
                    }
                }
                void handle_read() {
                    log_info("handle read");
                    if(conn_state_ == ConnState::HandShaking) {
                        handle_handshake();
                    }
                    auto bytes = ip::tcp::sockets::readable(socket_.fd());
                    if(bytes == 0) {
                        return;
                    }
                    recv_buffer_->enable_bytes(bytes);
                    bytes = socket_.recv(recv_buffer_->end(), bytes);
                    if(bytes == 0) {
                        log_info("read 0 bytes, close connection...", name_);
                        handle_close();
                    }
                    else if(bytes == -1) {
                        if(errno == EINTR || errno == EAGAIN) {
                            handle_read();
                        }
                        else {
                            handle_close();
                        }
                    }
                    else {
                        recv_buffer_->retrieve_write_bytes(bytes);
                        if(read_cb_) {
                            read_cb_(this->shared_from_this());
                        }
                    }
                }
                void handle_write() {
                    // 虽然进入handle_write表明fd可写，而handle_handshake也是检查fd是否可写
                    // 但是对于SslSocket而言还需要在handle_handshake中执行SSL_connect
                    // 所以这里不能省略
                    if(conn_state_ == ConnState::HandShaking) {
                        handle_handshake();
                    }
                    if(!send_buffer_->empty()) {
                        auto bytes = send_buffer_->size();
                        auto send_bytes = socket_.send(send_buffer_->begin(), bytes);
                        if(send_bytes == -1) {
                            if(errno == EINTR || errno == EAGAIN) {
                                // FIXME: 对于SSL是否也是如此 ?
                                handle_write();
                            }
                            else {
                                log_error("send return -1, close connection...");
                                handle_close();
                            }
                        }
                        else if(send_bytes == 0) {
                            handle_close();
                        }
                        else if(bytes == send_bytes) {
                            send_buffer_->clear();
                            if(sendfile_) {
                                socket_.set_write_callback(std::bind(&Connection::handle_sendfile, this));
                                handle_sendfile();
                            }
                            else {
                                if(write_cb_) {
                                    write_cb_(this->shared_from_this());
                                }
                            }
                        }
                        else {
                            send_buffer_->retrieve_read_bytes(send_bytes);
                            socket_.set_write_callback(std::bind(&Connection::handle_write, this));
                        }
                    }
                }
                void handle_close() {
                    /* is_closed_ = true; */
                    conn_state_ = ConnState::Closed;
                    socket_.disable_all();
                    if(close_cb_)
                        close_cb_(this->shared_from_this());
                }
                void handle_sendfile() {
                    if(!sendfile_) {
                        return;
                    }
                    int bytes = ip::tcp::sockets::sendfile(socket_.fd(), filename_, fileoffet_, filesize_);
                    if(bytes == 0) {
                        handle_close();
                    }
                    else if(bytes == -1) {
                        if(errno == EINTR || errno == EAGAIN) {
                            handle_sendfile();
                        }
                        else {
                            handle_close();
                        }
                    }
                    else if(bytes < static_cast<int>(filesize_)) {
                        socket_.set_write_callback(std::bind(&Connection::handle_sendfile, this));
                        fileoffet_ += bytes;
                        filesize_ -= bytes;
                    }
                    else {
                        sendfile_ = false;
                        filesize_ = 0;
                        fileoffet_ = 0;
                    }
                }
            protected:
                std::string name_;
                // 通过读取文件内容发送数据而非直接调用::sendfile时使用
                std::ifstream fin;
                std::string filename_;
                std::size_t filesize_{ 0 };
                off_t fileoffet_{ 0 };
                bool sendfile_{ false };

                EventLoop* loop_;
                // socket_t可以是TcpSocket或者SslSocket
                socket_t socket_;
                MessageCallBack read_cb_, write_cb_;
                ErrorCallBack error_cb_;
                CloseCallBack close_cb_;
                std::shared_ptr<Buffer> recv_buffer_, send_buffer_;

                ConnState conn_state_ { ConnState::Closed };
            private:
                static_assert(std::is_same_v<Socket, TcpSocket>
#ifdef CORTONO_USE_SSL
                    || std::is_same_v<Socket, SslSocket>
#endif
                    , "Socket is not TcpSocket or SSLSocket");
        };

    using TcpConnection = Connection<TcpSocket>;
#ifdef CORTONO_USE_SSL
    using SslConnection = Connection<SslSocket>;
#endif
}
