#pragma once

#include "sr_header.hpp"
#include "parser_module.hpp"
#include "utility.hpp"

namespace cortono
{
    namespace detail
    {
        template <typename... Middlewares>
        struct middlewares_tuple_wrapper
        {
            template <std::size_t Idx, typename Tuple, typename Middleware, typename... Args>
            Middleware middleware_helper(Tuple& t, Args... args) {
                if constexpr (sizeof...(Args) == std::tuple_size_v<Tuple>) {
                    return Middleware(args...);
                }
                else {
                    using arg_t = std::tuple_element_t<Idx, Tuple>;
                    return middleware_helper
                                <Idx + 1, Tuple, Middleware, Args..., arg_t>
                                    (t, args..., std::get<Idx>(t));
                }
            }
            template <std::size_t Idx, typename Tuple, typename... Args>
            std::tuple<Middlewares...> middlewares_tuple_helper(Tuple& t, Args&&... args) {
                if constexpr (sizeof...(Args) == sizeof...(Middlewares)) {
                    return std::make_tuple(std::forward<Args>(args)...);
                }
                else {
                    using middleware_t = std::tuple_element_t<Idx, std::tuple<Middlewares...>>;
                    return middlewares_tuple_helper
                                <Idx + 1, Tuple, Args..., middleware_t>
                                    (t, std::forward<Args>(args)..., std::move(middleware_helper<0, Tuple, middleware_t>(t)));
                }
            }
            template <typename... Args>
            std::tuple<Middlewares...> operator()(Args... args) {
                std::tuple<Args...> t(args...);
                return middlewares_tuple_helper<0, std::tuple<Args...>>(t);
            }
        };
    }

    template <typename Packet, typename... Middlewares>
    class Connection : public cortono::net::UdpConnection
    {
        public:
            using parent_t = cortono::net::UdpConnection;
            using packet_t = Packet;

            Connection(cortono::net::EventLoop* loop, int fd, const std::string& ip, std::uint16_t port)
                : parent_t(fd),
                  loop_(loop),
                  ip_(ip),
                  port_(port),
                  middlewares_(detail::middlewares_tuple_wrapper<Middlewares...>{}(loop_, ip, port))
            {
                auto sender = [this](const std::string& packet_str, const std::string& ip, std::uint16_t port) {
                    this->parent_t::send(packet_str.data(), packet_str.size(), ip, port);
                };

                black_magic::foreach(middlewares_, [this, sender = std::move(sender)](auto& module_obj){
                    if constexpr (black_magic::has_bind_sender<decltype(module_obj), decltype(sender)>::value) {
                        module_obj.bind_sender(sender);
                    }
                }, std::make_index_sequence<sizeof...(Middlewares)>{});
                /* }, std::make_index_sequence<std::tuple_size_v<decltype(middlewares_)>>{}); */
            }

            bool handle_read() {
                char buffer[packet_t::MAX_SIZE + 1] = "\0";
                std::string src_ip;
                std::uint16_t src_port;
                int read_bytes = this->parent_t::recv(buffer, sizeof(buffer) - 1, src_ip, src_port);
                if(read_bytes <= 0) {
                    return false;
                }
                log_info(read_bytes);
                packet_t packet(ip_, port_, src_ip, src_port, ip_, port_);
                if(!packet.feed(buffer, read_bytes)) {
                    log_info("invalid packet");
                    return false;
                }
                if(packet.is_recv_ack_packet()) {
                    log_info("recv ack:", packet.ack());
                }
                else if(packet.is_recv_data_packet()) {
                    log_info("recv data:", packet.seq(), "to", packet.seq() + packet.data_size());
                }
                else {
                    log_fatal("error, should be recv packet");
                    return false;
                }
                return handle_packet(packet);
            }
            bool handle_packet(packet_t& packet) {
                bool error = false;
                black_magic::foreach(middlewares_, [&error, &packet](auto& module_obj) {
                    if(!module_obj.check(packet)) {
                        error = true;
                    }
                }, std::make_index_sequence<sizeof...(Middlewares)>{});
                /* }, std::make_index_sequence<std::tuple_size_v<decltype(middlewares_)>>{}); */

                if(error) {
                    log_info("error, maybe the send window is full if your operation is to send packet");
                    return false;
                }
                black_magic::foreach(middlewares_, [&packet](auto& module_obj) {
                    module_obj.handle(packet);
                }, std::make_index_sequence<sizeof...(Middlewares)>{});
                /* }, std::make_index_sequence<std::tuple_size_v<decltype(middlewares_)>>{}); */

                if(packet.is_error_packet()) {
                    log_info("error packet, maybe the packet is lost");
                    return false;
                }
                else if(packet.is_send_ack_packet()) {
                    log_info("send ack:", packet.ack());
                }
                else if(packet.is_send_data_packet()) {
                    log_info("send data:", packet.seq(), "to", packet.seq() + packet.data_size());
                }
                else {
                    /* log_info("don't send anything, maybe recv ack packet, return"); */
                    return true;
                }
                packet.print();
                std::string packet_str = packet.to_string();
                this->parent_t::send(packet_str.data(), packet_str.size(), ip_, packet.des_port());
                log_info("......................................");
                return true;
            }

            template <typename Func>
            void run(Func&& f) {
                std::forward<Func>(f)();
            }
            template <typename Middleware>
            constexpr Middleware& get_middleware()  {
                /* return std::get<Middleware>(middlewares_); */
                return black_magic::get_element_ref_from_tuple_by_type<Middleware>(middlewares_);
            }

        private:
            cortono::net::EventLoop* loop_;

            std::string ip_;
            std::uint16_t port_;

            /* parser_t parser_; */
            std::tuple<Middlewares...> middlewares_;
    };
}
