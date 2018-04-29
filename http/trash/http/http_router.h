#pragma once

#include "../../cortono.hpp"
#include "utils.h"
#include <unordered_map>
#include <functional>
#include <type_traits>
#include <tuple>
#include <vector>

namespace cortono::http
{
    class request;
    class response;

    class http_router
    {
        public:
            template <http_method... Methods, typename Function, typename... AP>
                void register_handler(std::string_view source, Function&& f, AP&&... ap) {
                    if constexpr(sizeof...(Methods) > 0) {
                        auto source_arr = get_array<Methods...>(source);
                        for(auto& s : source_arr) {
                            register_nonmember_func(s, std::forward<Function>(f), std::forward<AP>(ap)...);
                        }
                    }
                }



            bool route(std::string_view method, std::string_view url, const request& req, response& res) {
                std::string key{ method.data(), method.length() };
                key += std::string{ url.data(), url.length() };
                if(auto it = map_invokers_.find(key); it != map_invokers_.end()) {
                    it->second(req, res);
                    return true;
                }
                log_error("no handle for ", key);
                return false;
            }
        private:

            template <typename Function, typename... AP>
            void register_nonmember_func(const std::string& source, Function f, AP&&... ap) {
                map_invokers_[source] = std::bind(&http_router::invoker<Function, AP...>, this,
                        std::placeholders::_1, std::placeholders::_2, std::move(f), std::move(ap)...);
            }

            template <typename Function, typename... AP>
            void invoker(const request& req, response& res, Function& f, AP&&... ap) {
                using result_type = typename std::invoke_result_t<Function, const request&, response&>;
                std::tuple<AP...> tp(std::move(ap)...);
                bool r = do_ap_before(req, res, tp);
                if(!r)
                    return;

                if constexpr(std::is_void_v<result_type>) {
                    f(req, res);
                    do_void_after(req, res, tp);
                }
                else {
                    result_type result = f(req, res);
                    do_after(std::move(result), req, res, tp);
                }
            }

            template <typename Tuple>
            bool do_ap_before(const request& req, response& res, Tuple& tp) {
                bool r{true};
                for_each_l(tp, [&r, &req, &res] (auto& item) {
                    if constexpr(has_before<decltype(item), const request&, response&>::value) {
                        r = item.before(req, res);
                    }
                /* make_index_sequence用于在编译期生成序列，如
                 * std::make_index_sequnce<3>{}会返回{0, 1, 2} */
                /* std::tuple_size提供编译期获取模板参数成员个数的能力 */
                /* std::tuple_size_v<T>相当于std::tuple_size<T>::value，其它同理 */
                }, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
                return r;
            }

            template <typename Tuple>
            void do_void_after(const request& req, response& res, Tuple& tp) {
                bool r{true};
                for_each_r(tp, [&r, &req, &res] (auto& item) {
                    if(!r)
                        return;
                    if constexpr(has_after<decltype(item), const request&, response&>::value) {
                        r = item.after(req, res);
                    }
                }, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
            }

            template <typename T, typename Tuple>
            void do_after(T&& result, const request& req, response& res, Tuple& tp) {
                bool r{true};
                for_each_r(tp, [&r, result = std::move(result), &req, &res] (auto& item) {
                    if(!r)
                        return;
                    if constexpr(has_after<decltype(item), const request&, response&>::value) {
                        r = item.after(std::move(result), req, res);
                    }
                }, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
            }
        private:
            using invoker_function = std::function<void(const request&, response&)>;
            std::unordered_map<std::string, invoker_function> map_invokers_;
    };
}
