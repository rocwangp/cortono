#pragma once

#include "../ip/sockets.hpp"
#include "connection.hpp"
#include "ssl_socket.hpp"

namespace cortono::net
{
/* #ifdef CORTONO_USE_SSL */
/*     class SSLConnection : public TcpConnection */
/*     { */
/*         public: */
/*             using ssl_sockets = ip::tcp::ssl; */

/*             SSLConnection(EventLoop* loop, int fd, ::SSL* ssl) */
/*                 : TcpConnection(loop, fd), */
/*                   ssl_(ssl) */
/*             { */
/*             } */
/*             ~SSLConnection() { */
/*                 ssl_sockets::close(ssl_); */
/*             } */

/*             virtual void send(const std::string& msg) virtual { */
/*                 if(msg.empty()) { */
/*                     return; */
/*                 } */
/*                 if(!send_buffer_->empty()) { */
/*                     send_buffer_->append(msg); */
/*                     return; */
/*                 } */
/*                 auto bytes = ip::tcp::ssl::send(socket_.ssl(), msg.data(), msg.size()); */
/*                 if(bytes == 0) { */
/*                     handle_close(); */
/*                 } */
/*                 else if(bytes == -1) { */
/*                     if(errno == EINTR || errno == EAGAIN) { */
/*                         send(msg); */
/*                     } */
/*                 } */
/*                 else if(bytes != static_cast<int>(msg.size())) { */
/*                     socket_.set_write_callback(std::bind(&TcpConnection::handle_write, this)); */
/*                     send_buffer_->append(msg.substr(bytes)); */
/*                 } */
/*             } */
/*             void handle_read() { */
/*                 auto bytes = ip::tcp::sockets::readable(socket_.fd()); */
/*                 recv_buffer_->enable_bytes(bytes); */
/*                 bytes = ip::tcp::ssl::recv(ssl_, recv_buffer_->end(), bytes); */
/*                 if(bytes == 0) { */
/*                     log_info("close connection"); */
/*                     handle_close(); */
/*                 } */
/*                 else if(bytes == -1) { */
/*                     log_error("read error", std::strerror(errno)); */
/*                     handle_read(); */
/*                 } */
/*                 else { */
/*                     recv_buffer_->retrieve_write_bytes(bytes); */
/*                     if(read_cb_) { */
/*                         log_info("read callback"); */
/*                         read_cb_(shared_from_this()); */
/*                     } */
/*                 } */
/*             } */
/*         private: */
/*             void handle_write() { */
/*                 if(!send_buffer_->empty()) { */
/*                     auto bytes = ip::tcp::ssl::send(socket_.ssl(), */
/*                                                     send_buffer_->begin(), */
/*                                                     send_buffer_->readable()); */
/*                     if(bytes == -1) { */
/*                         if(errno == EINTR || errno == EAGAIN) { */
/*                             handle_write(); */
/*                         } */
/*                         else { */
/*                             handle_close(); */
/*                         } */
/*                     } */
/*                     else if(bytes == 0) { */
/*                         handle_close(); */
/*                     } */
/*                     else if(bytes == send_buffer_->readable()) { */
/*                         send_buffer_->clear(); */
/*                         if(sendfile_) { */
/*                             socket_.set_write_callback(std::bind(&TcpConnection::handle_sendfile, this)); */
/*                             handle_sendfile(); */
/*                         } */
/*                         else { */
/*                             socket_.disable_writing(); */
/*                             if(write_cb_) { */
/*                                 write_cb_(shared_from_this()); */
/*                             } */
/*                         } */
/*                     } */
/*                     else { */
/*                         send_buffer_->retrieve_read_bytes(bytes); */
/*                         socket_.enable_writing(); */
/*                         socket_.set_write_callback(std::bind(&TcpConnection::handle_write, this)); */
/*                     } */
/*                 } */
/*             } */
/*         private: */
/*             ::SSL* ssl_; */
/*     }; */
/* #endif */
}
