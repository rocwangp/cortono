#pragma once

#include "../std.hpp"
#include "../util/noncopyable.hpp"
#include "socket.hpp"
#include "ssl_socket.hpp"
#include "eventloop.hpp"

namespace cortono::net
{
        template <typename Socket>
        class Connection : public std::enable_shared_from_this<Connection<Socket>>
        {
            public:
                using socket_t = Socket;

                typedef std::shared_ptr<Connection<Socket>>             Pointer;
                typedef std::function<void(const Connection::Pointer&)> MessageCallBack;
                typedef Connection::MessageCallBack                     CloseCallBack;
                typedef Connection::MessageCallBack                     ErrorCallBack;

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
                    socket_.enable_writing();
                }

                void reset_to_ssl() {
                    if(std::is_same_v<Socket, TcpSocket>) {
                        socket_.reset_to_ssl_socket();
                    }
                }
                void close() {
                    if(!is_closed) { handle_close(); }
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
                    if(!send_buffer_->empty()) {
                        send_buffer_->append(buffer, len);
                        return;
                    }
                    auto bytes = socket_.send(buffer, len);
                    if(bytes == 0) {
                        log_info("send return 0, close connection...");
                        handle_close();
                    }
                    else if(bytes == -1) {
                        /* if(errno == EINTR || errno == EAGAIN) { */
                            /* log_info("send is -1 and errno is EINTR | EAGAIN, call send again..."); */
                            /* /1* send(msg); *1/ */
                        /* } */
                    }
                    else if(bytes != static_cast<int>(len)) {
                        socket_.set_write_callback(std::bind(&Connection::handle_write, this));
                        send_buffer_->append(buffer + bytes, len - bytes);
                    }

                }
                void send(const std::string& msg) {
                    if(msg.empty()) {
                        return;
                    }
                    if(!send_buffer_->empty()) {
                        send_buffer_->append(msg);
                        return;
                    }
                    auto bytes = socket_.send(msg.data(), msg.size());
                    if(bytes == 0) {
                        log_info("send return 0, close connection...");
                        handle_close();
                    }
                    else if(bytes == -1) {
                        /* if(errno == EINTR || errno == EAGAIN) { */
                            /* log_info("send is -1 and errno is EINTR | EAGAIN, call send again..."); */
                            /* /1* send(msg); *1/ */
                        /* } */
                    }
                    else if(bytes != static_cast<int>(msg.size())) {
                        socket_.set_write_callback(std::bind(&Connection::handle_write, this));
                        send_buffer_->append(msg.substr(bytes));
                    }
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
                void handle_read() {
                    auto bytes = ip::tcp::sockets::readable(socket_.fd());
                    recv_buffer_->enable_bytes(bytes);
                    bytes = socket_.recv(recv_buffer_->end(), bytes);
                    if(bytes == 0) {
                        /* log_info("read 0 bytes, close connection...", name_); */
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
                            /* log_info("read callback", bytes); */
                            read_cb_(this->shared_from_this());
                        }
                    }
                }
                void handle_write() {
                    if(!send_buffer_->empty()) {
                        auto bytes = socket_.send(send_buffer_->begin(), send_buffer_->readable());
                        if(bytes == -1) {
                            if(errno == EINTR || errno == EAGAIN) {
                                handle_write();
                            }
                            else {
                                handle_close();
                            }
                        }
                        else if(bytes == 0) {
                            handle_close();
                        }
                        else if(bytes == send_buffer_->readable()) {
                            send_buffer_->clear();
                            if(sendfile_) {
                                socket_.set_write_callback(std::bind(&Connection::handle_sendfile, this));
                                handle_sendfile();
                            }
                            else {
                                socket_.disable_writing();
                                if(write_cb_) {
                                    write_cb_(this->shared_from_this());
                                }
                            }
                        }
                        else {
                            send_buffer_->retrieve_read_bytes(bytes);
                            socket_.set_write_callback(std::bind(&Connection::handle_write, this));
                        }
                    }
                }
                void handle_close() {
                    is_closed = true;
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
                std::string filename_;
                std::size_t filesize_{ 0 };
                off_t fileoffet_{ 0 };
                bool sendfile_{ false };

                EventLoop* loop_;
                socket_t socket_;
                MessageCallBack read_cb_, write_cb_;
                ErrorCallBack error_cb_;
                CloseCallBack close_cb_;
                std::shared_ptr<Buffer> recv_buffer_, send_buffer_;

                int buffer_size_ = 0;
                std::ifstream fin;

                bool is_closed{ false };

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
