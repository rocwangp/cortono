#pragma once

#include "../std.hpp"

namespace cortono::http::black_magic
{
    // 提取模板类的模板参数
    template <typename...>
    struct template_arg_traits;

    template <template <typename...> class ClassType, typename... Args>
    struct template_arg_traits<ClassType<Args...>>
    {
        template <std::size_t N>
        using arg_type = std::tuple_element_t<N, std::tuple<Args...>>;
    };

    // 提取函数的返回值，参数个数以及参数类型
    template <typename T>
    struct function_traits;

    // 对函数对象的特化版本
    template <typename ClassType, typename R, typename... Args>
    struct function_traits<R(ClassType::*)(Args...)>
    {
        static constexpr std::size_t arity = sizeof...(Args);
        using return_type = R;

        template <std::size_t N>
        using arg = std::tuple_element_t<N, std::tuple<Args...>>;
    };

    template <typename ClassType, typename R, typename... Args>
    struct function_traits<R(ClassType::*)(Args...) const>
    {
        static constexpr std::size_t arity = sizeof...(Args);
        using return_type = R;

        template <std::size_t N>
        using arg = std::tuple_element_t<N, std::tuple<Args...>>;
    };

    template <typename T>
    struct function_traits : public function_traits<decltype(&T::operator())>
    {
        using parent_t = function_traits<decltype(&T::operator())>;
        static constexpr std::size_t arity = parent_t::arity;
        using return_type = typename parent_t::return_type;

        template <std::size_t N>
        using arg = typename parent_t::template arg<N>;
    };

    // 获取第N个参数类型
    template <std::size_t N, typename... Args>
    struct get_nth_type
    {
        using type = std::tuple_element_t<N, std::tuple<Args...>>;
    };

    // 静态容器，保存类型，可在头尾插入
    template <typename... T>
    struct S
    {
        template <typename U>
        using push_front = S<U, T...>;
        template <typename U>
        using push_back = S<T..., U>;
    };

    template <typename T>
    struct promote
    {
        using type = T;
    };

    // 仅仅用于进行类型映射，因为url参数中只支持<int> <uint> <double> <string>四种
#define INTERNAL_TYPE_MAPPING(t1, t2) \
    template <> \
    struct promote<t1> \
    { \
        using type = t2; \
    };

    INTERNAL_TYPE_MAPPING(int, int64_t);
    INTERNAL_TYPE_MAPPING(short, int64_t);
    INTERNAL_TYPE_MAPPING(char, int64_t);
    INTERNAL_TYPE_MAPPING(long, int64_t);
    INTERNAL_TYPE_MAPPING(long long, int64_t);
    INTERNAL_TYPE_MAPPING(unsigned char, uint64_t);
    INTERNAL_TYPE_MAPPING(unsigned short, uint64_t);
    INTERNAL_TYPE_MAPPING(unsigned int, uint64_t);
    INTERNAL_TYPE_MAPPING(unsigned long, uint64_t);
    INTERNAL_TYPE_MAPPING(unsigned long long, uint64_t);
    INTERNAL_TYPE_MAPPING(float, double);

    template <typename T>
    using promote_t = typename promote<T>::type;
}

namespace cortono::http::utils
{
    struct nocase_compare
    {
        bool operator()(char ch1, char ch2) {
            return std::tolower(ch1) < std::tolower(ch2);
        }
    };
    struct ci_compare
    {
        bool operator()(const std::string& s1, const std::string& s2) const {
            return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), nocase_compare{});
        }
    };
    inline bool iequal(const char* src, int src_len, const char* des, int des_len) {
        for(int i = 0; i != std::min(src_len, des_len); ++i) {
            if(std::tolower(src[i]) != std::tolower(des[i])) {
                return false;
            }
        }
        return true;
    }

    inline bool iequal(const char* src, int src_len, const char* des) {
        return iequal(src, src_len, des, std::strlen(des));
    }

    inline bool iequal(const std::string& src, const char* des) {
        return iequal(src.data(), src.length(), des, std::strlen(des));
    }

    inline void to_lower(std::string& s) {
        for(auto& ch : s) {
            ch = std::tolower(ch);
        }
    }
}
