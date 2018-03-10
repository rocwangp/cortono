#pragma once

#include <type_traits>
#include <array>
#include <string>
#include <string_view>

namespace cortono::http
{
    enum class content_type {
        string,
        unknown
    };

    enum class http_method {
        DEL,
        GET,
        HEAD,
        POST,
        PUT,
        CONNECT,
        OPTIONS,
        TRACE
    };


    /* constexpr表示编译器常量，const仅仅是只读 */
    /* inline variable保证在多个编译单元中只有一个定义,
     * 用它可以将库文件全写成只包含.h的文件，无需考虑重复定义问题 */
    constexpr inline auto GET = http_method::GET;
    constexpr inline auto DEL = http_method::DEL;
    constexpr inline auto HEAD = http_method::HEAD;
    constexpr inline auto POST = http_method::POST;
    constexpr inline auto CONNECT = http_method::CONNECT;
    constexpr inline auto OPTIONS = http_method::OPTIONS;
    constexpr inline auto TRACE = http_method::TRACE;


    /* "GET"sv用于初始化一个std::string_view类型变量 */
    using namespace std::literals;
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::GET>) noexcept { return "GET"sv; }
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::DEL>) noexcept { return "DEL"sv; }
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::PUT>) noexcept { return "PUT"sv; }
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::POST>) noexcept { return "POST"sv; }
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::HEAD>) noexcept { return "HEAD"sv; }
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::TRACE>) noexcept { return "TRACE"sv; }
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::OPTIONS>) noexcept { return "OPTIONS"sv; }
    constexpr auto method_to_name(std::integral_constant<http_method, http_method::CONNECT>) noexcept { return "CONNECT"sv; }


    template <http_method Method>
    auto get_string(std::string& name, std::string_view source) {
        name = method_to_name(std::integral_constant<http_method, Method>{});
        name += std::string(source.data(), source.length());
        return name;
    }

    template <http_method... Methods>
    auto get_array(std::string_view source) {
        std::array<std::string, sizeof...(Methods)> arr{};
        size_t index{0};
        /* fold expression，相当于将参数列表中的每一个传给get_string的模板参数，调用多次get_string */
        (get_string<Methods>(arr[index++], source), ...);
        return arr;
    }


/* 判断一个类中一否存在某个成员函数，利用模板参数推倒的SFINAE原则（替换失败并非错误）*/
    /* 提供两个模板函数Check，编译器会选择最优匹配的那个，
     * 当调用Check(0)时，由于0是int类型所以会优先尝试推倒第一个重载，
     * 随后尝试调用U.member()函数，(注，这里的member是#define的参数，不是成员函数名)
     * 如果类中存在member函数，那么第一个Check推倒成功，否则推倒第二个。
     * decltype用于获取变量的类型，由于结果只需要是true和false，所以采用decltype(a, b)的形式，
     * 逗号表达式会返回最后一个值，所以相当于decltype(b)，
     * 所以如果推倒成功则返回std::true_type，否则推倒第二个返回std::false_type */
#define HAS_MEMBER(member)\
    template <typename T, typename... Args>\
    struct has_##member\
    {\
        private:\
            template <typename U> \
                static auto Check(int) ->decltype(std::declval<U>().member(std::declval<Args>()...), std::true_type{});\
            template <typename U>\
                static std::false_type Check(...);\
        public:\
               enum { value = std::is_same<decltype(Check<T>(0)), std::true_type>::value };\
    };\

    HAS_MEMBER(before);
    HAS_MEMBER(after);


    template <typename... Args, typename Function, std::size_t... Idx>
        constexpr void for_each_l(std::tuple<Args...>& t, Function&& f, std::index_sequence<Idx...>) {
            (std::forward<Function>(f)(std::get<Idx>(t)), ...);
        }

    template <typename... Args, typename Function, std::size_t... Idx>
        constexpr void for_each_r(std::tuple<Args...>& t, Function&& f, std::index_sequence<Idx...>) {
            constexpr auto size = sizeof...(Idx);
            (std::forward<Function>(f)(std::get<size - Idx - 1>(t)), ...);
        }


    struct ci_less
    {
        struct nocase_compare
        {
            bool operator()(const unsigned char& c1, const unsigned char& c2) const {
                return std::tolower(c1) < std::tolower(c2);
            }
        };

        bool operator()(const std::string& s1, const std::string& s2)const  {
            return std::lexicographical_compare(
                s1.begin(), s1.end(), s2.begin(), s2.end(),
                nocase_compare());
        }

        bool operator()(std::string_view sv1, std::string_view sv2) const {
            return std::lexicographical_compare(
                sv1.begin(), sv1.end(), sv2.begin(), sv2.end(),
                nocase_compare());
        }
    };

    inline bool iequal(const char* s1, std::size_t l, const char* s2) {
        for(std::size_t i = 0; i < l; ++i) {
            if(std::tolower(s1[i]) != std::tolower(s2[i]))
              return false;
        }
        return true;
    }
}
