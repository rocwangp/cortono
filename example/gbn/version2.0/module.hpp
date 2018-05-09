#pragma once

#include "../../../cortono.hpp"
#include "parser_module.hpp"

namespace cortono
{

#define HAS_MEMBER(member) \
    template <typename T, typename... Args>\
    struct has_##member {\
        private:\
            template <typename U>\
            static auto check(int) ->decltype(std::declval<U>().member(std::declval<Args>()...), std::true_type{});\
            template <typename U>\
            static std::false_type check(...);\
        public:\
            enum { value = std::is_same<decltype(check<T>(0)), std::true_type>::value };\
    };\

    HAS_MEMBER(bind_sender);


    template <typename Tuple, typename Func, std::size_t... Idx>
    constexpr void foreach(Tuple& t, Func&& f, std::index_sequence<Idx...>) {
        (f(std::get<Idx>(t)), ...);
    }

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
    template <std::size_t Idx, typename Type, typename Tuple>
    Type& get_element_ref_by_type_helper(Tuple& t) {
        if constexpr (Idx == std::tuple_size_v<Tuple>) {
            log_fatal("no type:", typeid(Type).name(), "in tuple");
        }
        if constexpr (std::is_same_v<Type, std::tuple_element_t<Idx, Tuple>>) {
            return std::get<Idx>(t);
        }
        else {
            return get_element_ref_by_type_helper<Idx + 1, Type, Tuple>(t);
        }
    }

    template <typename Type, typename Tuple>
    Type& get_element_ref_by_type(Tuple& t) {
        return get_element_ref_by_type_helper<0, Type, Tuple>(t);
    }

    template <typename... Middlewares>
    class Connection : public cortono::net::UdpConnection
    {
        public:
            using parent_t = cortono::net::UdpConnection;
            using parser_t = ParserModule<std::uint32_t>;
            using packet_t = MsgPacket<std::uint32_t>;

            Connection(cortono::net::EventLoop* loop, int fd, const std::string& ip, std::uint16_t port)
                : parent_t(fd),
                  loop_(loop),
                  ip_(ip),
                  port_(port),
                  middlewares_(middlewares_tuple_wrapper<Middlewares...>{}(loop_, ip, port))
            {
                std::function<void(const std::string&, const std::string&, std::uint16_t)> sender =
                    [this](const std::string& packet_str, const std::string& ip, std::uint16_t port) {
                    this->parent_t::send(packet_str.data(), packet_str.size(), ip, port);
                };

                foreach(middlewares_, [this, sender = std::move(sender)](auto& module_obj){
                    if constexpr (has_bind_sender<decltype(module_obj), decltype(sender)>::value) {
                        module_obj.bind_sender(sender);
                    }
                }, std::make_index_sequence<std::tuple_size_v<decltype(middlewares_)>>{});
            }

            void handle_read() {
                char buffer[MsgPacket<std::uint32_t>::MAX_SIZE + 1] = "\0";
                std::string ip;
                std::uint16_t port;
                int read_bytes = this->parent_t::recv(buffer, sizeof(buffer) - 1, ip, port);
                log_info(read_bytes, buffer);
                if(read_bytes <= 0) {
                    return;
                }
                parser_t parser(ip_, port_);
                log_trace;
                if(!parser.feed(buffer, read_bytes)) {
                    log_info("parser error");
                    return;
                }
                if(parser.is_recv_ack_packet()) {
                    log_info("recv ack:", parser.ack());
                }
                else if(parser.is_recv_data_packet()) {
                    log_info("recv data:", parser.seq(), "to", parser.seq() + parser.data_size());
                }
                else {
                    log_fatal("error, should be recv packet");
                }
                log_trace;
                handle_packet(parser);
            }
            template <typename Func>
            void run(Func&& f) {
                f();
            }
            template <typename Middleware>
            Middleware& get_middleware() const {
                return get_element_ref_by_type<Middleware>(middlewares_);
            }

            bool handle_packet(parser_t& parser) {
                log_trace;
                bool error = false;
                foreach(middlewares_, [&error, &parser](auto& module_obj) {
                    if(!module_obj.check(parser)) {
                        error = true;
                    }
                }, std::make_index_sequence<std::tuple_size_v<decltype(middlewares_)>>{});

                if(error) {
                    log_info("error, maybe the send window is full if your operation is to send packet");
                    return false;
                }
                foreach(middlewares_, [&parser](auto& module_obj) {
                    module_obj.handle(parser);
                }, std::make_index_sequence<std::tuple_size_v<decltype(middlewares_)>>{});

                parser.print_packet();
                if(parser.is_send_ack_packet()) {
                    log_info("send ack:", parser.ack());
                }
                else if(parser.is_send_data_packet()) {
                    log_info("send data:", parser.seq(), "to", parser.seq() + parser.data_size());
                }
                else {
                    log_info("don't send anything, return");
                    return true;
                }
                log_trace;
                log_info(ip_, port_, parser.des_port());
                parser.print_packet();
                std::string packet_str = parser.to_string();
                log_info(packet_str);
                this->parent_t::send(packet_str.data(), packet_str.size(), ip_, parser.des_port());
                log_trace;
                return true;
            }
        private:
            cortono::net::EventLoop* loop_;

            std::string ip_;
            std::uint16_t port_;

            /* parser_t parser_; */
            std::tuple<Middlewares...> middlewares_;
    };
}
