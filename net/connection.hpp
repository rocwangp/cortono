#pragma once

#include "../std.hpp"
#include "socket.hpp"
#include "eventloop.hpp"

namespace cortono::net
{
    class TcpConnection : public std::enable_shared_from_this<TcpConnection>
    {
        public:
            typedef std::shared_ptr<TcpConnection>                     Pointer;
            typedef std::function<void(const TcpConnection::Pointer&)> MessageCallBack;
            typedef TcpConnection::MessageCallBack                     CloseCallBack;
            typedef TcpConnection::MessageCallBack                     ErrorCallBack;

            TcpConnection(EventLoop* loop, int fd)
                : loop_(loop),
                  socket_(fd),
                  recv_buffer_(std::make_shared<Buffer>()),
                  send_buffer_(std::make_shared<Buffer>())
            {
                name_ = socket_.local_address() + ":" + socket_.peer_address();
                socket_.tie(loop_->poller());
                socket_.set_read_callback(std::bind(&TcpConnection::handle_read, this));
                socket_.set_close_callback(std::bind(&TcpConnection::handle_close, this));
                socket_.set_write_callback(std::bind(&TcpConnection::handle_write, this));
                socket_.enable_reading();
                socket_.enable_writing();
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
            std::string_view name() {
                if(name_.empty()) {
                    return {};
                }
                else {
                    return { name_.data(), name_.length() };
                }
            }
            void close() {
                handle_close();
            }
            void send(const std::string& msg) {
                if(msg.empty()) {
                    return;
                }
                if(!send_buffer_->empty()) {
                    send_buffer_->append(msg);
                    return;
                }
                auto bytes = ip::tcp::sockets::send(socket_.fd(), msg.data(), msg.size());
                if(bytes == 0) {
                    handle_close();
                }
                else if(bytes == -1) {
                    if(errno == EINTR || errno == EAGAIN) {
                        send(msg);
                    }
                }
                else if(bytes != static_cast<int>(msg.size())) {
                    socket_.set_write_callback(std::bind(&TcpConnection::handle_write, this));
                    send_buffer_->append(msg.substr(bytes));
                }
            }
            void sendfile(std::string_view filename) {
                if(filename.empty()) {
                    return;
                }
                filename_ = std::string{ filename.data(), filename.length() };
                fileoffet_ = 0;
                filesize_ = util::get_filesize(filename);
                if(filesize_ == 0) {
                    return;
                }
                sendfile_ = true;
                fin.open(filename_, std::ios_base::in);
                if(!send_buffer_->empty()) {
                    return;
                }
                handle_sendfile();
            }
            void read_file_to_send() {
                socket_.disable_writing();
                socket_.enable_writing();
                socket_.set_write_callback([this]{
                    int write_bytes = 0;
                    do {
                        if(buffer_size_ == 0) {
                            buffer_size_ = fin.readsome(file_buffer_, sizeof(file_buffer_));
                        }
                        if(buffer_size_ == 0) {
                            log_debug("sendfile done");
                            socket_.disable_writing();
                            fin.close();
                            break;
                        }
                        write_bytes = ip::tcp::sockets::send(socket_.fd(), file_buffer_, buffer_size_);
                        buffer_size_ -= std::max(0, write_bytes);
                    }while(write_bytes == buffer_size_);
                });
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
        private:
            void handle_read() {
                auto bytes = ip::tcp::sockets::readable(socket_.fd());
                recv_buffer_->enable_bytes(bytes);
                bytes = ip::tcp::sockets::recv(socket_.fd(), recv_buffer_->end(), bytes);
                if(bytes == 0) {
                    log_info("close connection");
                    handle_close();
                }
                else if(bytes == -1) {
                    log_error("read error");
                    handle_read();
                }
                else {
                    recv_buffer_->retrieve_write_bytes(bytes);
                    if(read_cb_) {
                        read_cb_(shared_from_this());
                    }
                }
            }
            void handle_write() {
                if(!send_buffer_->empty()) {
                    auto bytes = ip::tcp::sockets::send(socket_.fd(),
                                                        send_buffer_->begin(),
                                                        send_buffer_->readable());
                    if(bytes == -1) {
                        if(errno == EINTR || errno == EAGAIN) {
                            handle_write();
                        }
                    }
                    else if(bytes == 0) {
                        handle_close();
                    }
                    else if(bytes == send_buffer_->readable()) {
                        send_buffer_->clear();
                        if(sendfile_) {
                            socket_.set_write_callback(std::bind(&TcpConnection::handle_sendfile, this));
                        }
                        else {
                            socket_.disable_writing();
                            if(write_cb_) {
                                write_cb_(shared_from_this());
                            }
                        }
                    }
                    else {
                        send_buffer_->retrieve_read_bytes(bytes);
                    }
                }
            }
            void handle_close() {
                socket_.disable_all();
                close_cb_(shared_from_this());
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
                }
                else if(bytes < static_cast<int>(filesize_)) {
                    socket_.set_write_callback(std::bind(&TcpConnection::handle_sendfile, this));
                    fileoffet_ += bytes;
                    filesize_ -= bytes;
                }
                else {
                    sendfile_ = false;
                }
            }
            void handle_none() {

            }
        private:
            std::string name_;
            std::string filename_;
            std::size_t filesize_ = 0;
            off_t fileoffet_ = 0;
            bool sendfile_ = false;

            EventLoop* loop_;
            TcpSocket socket_;
            MessageCallBack read_cb_, write_cb_;
            ErrorCallBack error_cb_;
            CloseCallBack close_cb_;
            std::shared_ptr<Buffer> recv_buffer_, send_buffer_;

            char file_buffer_[128 * 1024];
            int buffer_size_ = 0;
            std::ifstream fin;
    };
}
