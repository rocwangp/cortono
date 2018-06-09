#pragma once

#include "sr_header.hpp"

namespace black_magic
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

    template <std::uint64_t N, std::uint64_t M>
    struct Power
    {
        static const std::uint64_t value = N * Power<N, M - 1>::value;
    };

    template <std::uint64_t N>
    struct Power<N, 0>
    {
        static const std::uint64_t value = 1;
    };

    template <std::uint64_t N, std::uint64_t M>
    struct RoundUp
    {
        static const std::uint64_t value = (N + (M - 1)) & (~(M - 1));
    };

    template <typename T>
    struct bits_traits;

#define INTERNAL_BITS_MAPPING(T, N) \
    template <> \
    struct bits_traits<T> \
    {\
        static const std::uint16_t value = N; \
    };


    INTERNAL_BITS_MAPPING(std::uint16_t, 16);
    INTERNAL_BITS_MAPPING(std::uint32_t, 32);
    INTERNAL_BITS_MAPPING(std::uint64_t, 64);

    template <typename T>
    static constexpr inline auto bits_traits_v = bits_traits<T>::value;


    template <std::uint16_t N>
    struct promote;

#define INTERNAL_TYPE_MAPPING(N, T)\
    template <> \
    struct promote<N> \
    {\
        using type = T;\
    };

    INTERNAL_TYPE_MAPPING(16, std::uint16_t);
    INTERNAL_TYPE_MAPPING(32, std::uint32_t);
    INTERNAL_TYPE_MAPPING(64, std::uint64_t);

    template <std::uint16_t N>
    using promote_t = typename promote<N>::type;

    /* template <std::size_t Idx, typename Type, typename Tuple> */
    /* Type& get_element_ref_by_type_helper(Tuple& t) { */
    /*     if (Idx == std::tuple_size_v<Tuple>) { */
    /*         log_fatal("no type:", typeid(Type).name(), "in tuple"); */
    /*     } */
    /*     if (std::is_same_v<Type, std::tuple_element_t<Idx, Tuple>>) { */
    /*         return std::get<Idx>(t); */
    /*     } */
    /*     else { */
    /*         return get_element_ref_by_type_helper<Idx + 1, Type, Tuple>(t); */
    /*     } */
    /* } */

    /* template <typename Type, typename Tuple> */
    /* Type& get_element_ref_by_type(Tuple& t) { */
    /*     return get_element_ref_by_type_helper<0, Type, Tuple>(t); */
    /* } */

    namespace detail
    {
        template <std::size_t Idx, typename T, typename... Args>
        struct get_index_of_element_from_tuple_by_type_impl
        {
            static const auto value = Idx;
        };

        template <std::size_t Idx, typename T, typename U, typename... Args>
        struct get_index_of_element_from_tuple_by_type_impl<Idx, T, U, Args...>
        {
            static const auto value = get_index_of_element_from_tuple_by_type_impl<Idx + 1, T, Args...>::value;
        };

        template <std::size_t Idx, typename T, typename... Args>
        struct get_index_of_element_from_tuple_by_type_impl<Idx, T, T, Args...>
        {
            static const auto value = Idx;
        };

        /* template <typename T, typename ... Args> */
        /* struct contain */
        /* { */
        /*     static constexpr auto value = std::false_type{}; */
        /* }; */

        /* template <typename T, typename... Args> */
        /* struct contain<T, T, Args...> */
        /* { */
        /*     static constexpr auto value = std::true_type{}; */
        /* }; */

        /* template <typename T, typename U, typename... Args> */
        /* struct contain<T, U, Args...> */
        /* { */
        /*     static const auto value = contain<T, Args...>::value; */
        /* }; */

        template <typename T, typename... Args>
        struct contains : std::false_type {};

        template <typename T, typename U, typename... Args>
        struct contains<T, U, Args...> :
            std::conditional_t<std::is_same_v<T, U>, std::true_type, contains<T, Args...>> {};
    }

    template <typename T, typename... Args>
    T& get_element_ref_from_tuple_by_type(std::tuple<Args...>& t) {
        static_assert(detail::contains<T, Args...>::value, "no target type in tuple");
        return std::get<detail::get_index_of_element_from_tuple_by_type_impl<0, T, Args...>::value>(t);
    }
}
