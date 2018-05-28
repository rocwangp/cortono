#pragma once

#include "../std.hpp"
#include "../cortono.hpp"
#include "http_utils.hpp"
#include "http_response.hpp"
#include "http_request.hpp"

namespace cortono::http
{
    enum class ParamType
    {
        INT,
        UINT,
        DOUBLE,
        STRING,
        PATH,

        PARAM_NUMS
    };

    struct routing_params
    {
        std::vector<int64_t> int_params;
        std::vector<uint64_t> uint_params;
        std::vector<double> double_params;
        std::vector<std::string> string_params;

        template <typename T>
        T get(std::size_t i) const {
            if constexpr (std::is_same_v<T, int64_t>) {
                return int_params[i];
            }
            else if constexpr (std::is_same_v<T, uint64_t>) {
                return uint_params[i];
            }
            else if constexpr (std::is_same_v<T, double>) {
                return double_params[i];
            }
            else {
                return string_params[i];
            }
        }
    };

    template <typename H>
    struct call_params
    {
        H& handle;
        const Request& req;
        Response& res;
        const routing_params& params;

    };
    template <typename T, std::size_t P>
    struct call_pair
    {
        using type = T;
        static constexpr std::size_t pos = P;
    };

    // std::vector<int64_t> std::vector<uint64_t> std::vector<double> std::string<std::string>四种容器中都存储若干数据
    // 给出目标函数的参数类型列表，如int64_t, std::string, double, int64_t, uint64_t, ....
    // 需要依次将容器中的数据传给目标函数，保证类型匹配
    template <typename F, int Nint, int Nuint, int Ndouble, int Nstring, typename S1, typename S2>
    struct call {};

    template <typename F, int Nint, int Nuint, int Ndouble, int Nstring, typename... Args1, typename... Args2>
    struct call<F, Nint, Nuint, Ndouble, Nstring, black_magic::S<int64_t, Args1...>, black_magic::S<Args2...>>
    {
        void operator()(F cparams) {
            using pushed = typename black_magic::S<Args2...>::template push_front<call_pair<int64_t, Nint>>;
            call<F, Nint + 1, Nuint, Ndouble, Nstring, black_magic::S<Args1...>, pushed>{}(cparams);
        }
    };
    template <typename F, int Nint, int Nuint, int Ndouble, int Nstring, typename... Args1, typename... Args2>
    struct call<F, Nint, Nuint, Ndouble, Nstring, black_magic::S<uint64_t, Args1...>, black_magic::S<Args2...>>
    {
        void operator()(F cparams) {
            using pushed = typename black_magic::S<Args2...>::template push_front<call_pair<uint64_t, Nuint>>;
            call<F, Nint, Nuint + 1, Ndouble, Nstring, black_magic::S<Args1...>, pushed>{}(cparams);
        }
    };
    template <typename F, int Nint, int Nuint, int Ndouble, int Nstring, typename... Args1, typename... Args2>
    struct call<F, Nint, Nuint, Ndouble, Nstring, black_magic::S<double, Args1...>, black_magic::S<Args2...>>
    {
        void operator()(F cparams) {
            using pushed = typename black_magic::S<Args2...>::template push_front<call_pair<double, Ndouble>>;
            call<F, Nint, Nuint, Ndouble + 1, Nstring, black_magic::S<Args1...>, pushed>{}(cparams);
        }
    };
    template <typename F, int Nint, int Nuint, int Ndouble, int Nstring, typename... Args1, typename... Args2>
    struct call<F, Nint, Nuint, Ndouble, Nstring, black_magic::S<std::string, Args1...>, black_magic::S<Args2...>>
    {
        void operator()(F cparams) {
            using pushed = typename black_magic::S<Args2...>::template push_front<call_pair<std::string, Nstring>>;
            call<F, Nint, Nuint, Ndouble, Nstring + 1, black_magic::S<Args1...>, pushed>{}(cparams);
        }
    };
    template <typename F, int Nint, int Nuint, int Ndouble, int Nstring, typename... Args>
    struct call<F, Nint, Nuint, Ndouble, Nstring, black_magic::S<>, black_magic::S<Args...>>
    {
        void operator()(F cparams) {
            cparams.handle(cparams.req,
                           cparams.res,
                           cparams.params.template get<typename Args::type>(Args::pos)...);
        }
    };

    // 目标函数的参数列表可以是
    //  1.const Request&, Response&, ...(int64_t, uint64_t, double, std::string中的一个或若干个(可重复))
    //  2.const Request&, ...(int64_t, uint64_t, double, std::string中的一个或若干个(可重复))
    //  3....(int64_t, uint64_t, double, std::string中的一个或若干个(可重复))
    // 将这三种函数类型统一包装成
    //   const Request&, Response&, ...(int64_t, uint64_t, double, std::string中的一个或若干个(可重复))
    // 且调用时仍然以原参数类型传参
    template <typename Func, typename... ArgsWrapper>
    struct function_wrapper
    {
        template <typename Req, typename... Args>
        struct req_handler_wrapper
        {
            req_handler_wrapper(Func func) : f(std::move(func)) {}

            void operator()(const Request& req, Response& res, Args... args) {
                res = Response(f(req, args...));
            }
            Func f;
        };

        template <typename... Args>
        void set_(Func f) {
            if constexpr (std::is_same_v<const Request&, std::tuple_element_t<0, std::tuple<Args..., void>>>) {
                if constexpr (std::is_same_v<Response&, std::tuple_element_t<1, std::tuple<Args..., void, void>>>) {
                    handler_ = std::move(f);
                }
                else {
                    handler_ = req_handler_wrapper<Args...>(std::move(f));
                }
            }
            else {
                handler_ = [f = std::move(f)](const Request&, Response& res, Args... args) {
                    res = Response(f(args...));
                };
            }
        }

        // 提取参数列表中非const Request&, Response&中的参数类型
        template <typename... Args>
        struct handler_type_helper
        {
            using type = std::function<void(const Request&, Response&, Args...)>;
            using args_type = black_magic::S<black_magic::promote_t<Args>...>;
        };

        template <typename... Args>
        struct handler_type_helper<const Request&, Args...>
        {
            using type = std::function<void(const Request&, Response&, Args...)>;
            using args_type = black_magic::S<black_magic::promote_t<Args>...>;
        };

        template <typename... Args>
        struct handler_type_helper<const Request&, Response&, Args...>
        {
            using type = std::function<void(const Request&, Response&, Args...)>;
            using args_type = black_magic::S<black_magic::promote_t<Args>...>;
        };

        typename handler_type_helper<ArgsWrapper...>::type handler_;

        void operator()(const Request& req, Response& res, const routing_params& params) {
            call<call_params<decltype(handler_)>,
                 0, 0, 0, 0,
                 typename handler_type_helper<ArgsWrapper...>::args_type,
                 black_magic::S<>>{}
            (call_params<decltype(handler_)>{ handler_, req, res, params });
        }
    };


    class DynamicRule
    {
        public:
            DynamicRule(std::string&& rule)
                : rule_(std::move(rule))
            {  }

            using self_t = DynamicRule;

            self_t& methods(HttpMethod method) {
                methods_.emplace_back(method);
                return *this;
            }

            template <typename Func>
            void operator()(Func f) {
                if(methods_.empty()) {
                    methods_.emplace_back(HttpMethod::GET);
                }
                using function_t = black_magic::function_traits<Func>;
                handler_ = wrap(std::move(f), std::make_index_sequence<function_t::arity>{});
            }

            template <typename Func, std::size_t... Indices>
            std::function<void(const Request&, Response&, const routing_params&)> wrap(Func f, std::index_sequence<Indices...>) {
                using function_t = black_magic::function_traits<Func>;
                auto ret = function_wrapper<Func, typename function_t::template arg<Indices>...>{};
                ret.template set_<typename function_t::template arg<Indices>...>(std::move(f));
                return ret;
            }

            void handle(const Request& req, Response& res, const routing_params& params) {
                handler_(req, res, params);
            }
            const std::string& rule() const {
                return rule_;
            }
            template <typename Func>
            void forearch_method(Func f) {
                for(auto& method : methods_) {
                    std::forward<Func>(f)(method);
                }
            }
        private:
            std::string rule_;
            std::vector<HttpMethod> methods_;
            std::function<void(const Request&, Response&, const routing_params&)> handler_;
    };


    class Trie
    {
        public:

            struct TrieNode
            {
                std::size_t rule_index{ 0 };
                std::array<unsigned, (int)ParamType::PARAM_NUMS> param_children;
                std::unordered_map<std::string, unsigned> children;
            };

            Trie() : nodes_(1) {}

            void add(const std::string& rule, std::size_t rule_index) {
                std::size_t idx{ 0 };
                for(std::size_t i = 0; i != rule.size(); ++i) {
                    char c = rule[i];
                    if(c == '<') {
                        static struct ParamTraits {
                            ParamType type;
                            std::string name;
                        }param_traits[] = {
                            { ParamType::INT, "<int>" },
                            { ParamType::UINT, "<uint>" },
                            { ParamType::DOUBLE, "<double>" },
                            { ParamType::STRING, "<string>" },
                            { ParamType::PATH, "<path>" }
                        };
                        for(auto& param_trait : param_traits) {
                            if(rule.compare(i, param_trait.name.size(), param_trait.name) == 0) {
                                std::string piece = rule.substr(i, param_trait.name.size());
                                if(!nodes_[idx].param_children[(int)(param_trait.type)]) {
                                    auto new_node_idx = new_node();
                                    nodes_[idx].param_children[(int)(param_trait.type)] = new_node_idx;
                                }
                                idx = nodes_[idx].param_children[(int)(param_trait.type)];
                                i += param_trait.name.size() - 1;
                                break;
                            }
                        }
                    }
                    else {
                        std::string piece(&c, 1);
                        if(!nodes_[idx].children.count(piece)) {
                            auto new_node_idx = new_node();
                            nodes_[idx].children.emplace(piece, new_node_idx);
                        }
                        idx = nodes_[idx].children[piece];
                    }
                }
                if(nodes_[idx].rule_index != 0) {
                    throw std::runtime_error("handler already exists for " + rule);
                }
                nodes_[idx].rule_index = rule_index;
            }
            std::pair<std::int32_t, routing_params> find(const std::string& req_url, std::size_t pos = 0, TrieNode* node = nullptr, routing_params* params = nullptr) {
                routing_params empty;
                if(params == nullptr) {
                    params = &empty;
                }
                if(pos == req_url.size()) {
                    return { node->rule_index, *params };
                }
                if(node == nullptr) {
                    node = &nodes_.front();
                }
                std::int32_t rule_index{ -1 };
                routing_params match_params;

                auto update_found([&](auto& ret) {
                    rule_index = ret.first;
                    match_params = std::move(ret.second);
                });
                if(node->param_children[(int)(ParamType::INT)]) {
                    char c = req_url[pos];
                    if((c >= '0' && c <= '9') || (c == '+' || c == '-')) {
                        char* eptr;
                        errno = 0;
                        int64_t value = std::strtoll(req_url.data() + pos, &eptr, 10);
                        if(errno != ERANGE) {
                            params->int_params.emplace_back(value);
                            auto ret = find(req_url, eptr - req_url.data(), &nodes_[node->param_children[(int)(ParamType::INT)]], params);
                            update_found(ret);
                            params->int_params.pop_back();
                        }
                    }
                }
                if(node->param_children[(int)(ParamType::UINT)]) {
                    char c = req_url[pos];
                    if((c >= '0' && c <= '9') || (c == '+')) {
                        char* eptr;
                        errno = 0;
                        uint64_t value = std::strtoull(req_url.data() + pos, &eptr, 10);
                        if(errno != ERANGE) {
                            params->uint_params.emplace_back(value);
                            auto ret = find(req_url, eptr - req_url.data(), &nodes_[node->param_children[(int)(ParamType::UINT)]], params);
                            update_found(ret);
                            params->uint_params.pop_back();
                        }
                    }
                }
                if(node->param_children[(int)(ParamType::DOUBLE)]) {
                    char c = req_url[pos];
                    if((c >= '0' && c <= '9') || (c == '-' || c == '+')) {
                        char* eptr;
                        errno = 0;
                        double value = std::strtold(req_url.data() + pos, &eptr);
                        if(errno != ERANGE) {
                            params->double_params.emplace_back(value);
                            auto ret = find(req_url, eptr - req_url.data(), &nodes_[node->param_children[(int)(ParamType::DOUBLE)]], params);
                            update_found(ret);
                            params->double_params.pop_back();
                        }
                    }
                }
                if(node->param_children[(int)(ParamType::STRING)]) {
                    std::size_t epos = req_url.find_first_of('/', pos);
                    if(epos == std::string::npos) {
                        epos = req_url.size();
                    }
                    if(epos != pos) {
                        params->string_params.emplace_back(req_url.substr(pos, epos - pos));
                        auto ret = find(req_url, epos, &nodes_[node->param_children[(int)(ParamType::STRING)]], params);
                        update_found(ret);
                        params->string_params.pop_back();
                    }
                }
                if(node->param_children[(int)(ParamType::PATH)]) {
                    params->string_params.emplace_back(req_url.substr(pos));
                    auto ret = find(req_url, req_url.size(), &nodes_[node->param_children[(int)(ParamType::PATH)]], params);
                    update_found(ret);
                    params->double_params.pop_back();
                }
                for(auto& [format, next_idx] : node->children) {
                    if(!format.empty() && req_url.compare(pos, format.size(), format) == 0) {
                        auto ret = find(req_url, pos + format.size(), &nodes_[next_idx], params);
                        update_found(ret);
                    }
                }
                return { rule_index, match_params };
            }
        private:
            unsigned new_node() {
                nodes_.emplace_back();
                return nodes_.size() - 1;
            }
        private:
            std::vector<TrieNode> nodes_;
    };

    class Router
    {
        public:
            DynamicRule& new_dynamic_rule(std::string&& rule) {
                auto dynamic_rule = new DynamicRule(std::move(rule));
                all_rules_.emplace_back(dynamic_rule);
                return *dynamic_rule;
            }
            void volidate() {
                for(auto& rule : all_rules_) {
                    internal_add_rule_object(rule->rule(), rule.get());
                }
            }
            void handle(const Request& req, Response& res) {
                /* auto [rule_index, params] = method_rules_[(int)req.method].trie.find(req.url); */
                /* method_rules_[(int)req.method].rules[rule_index]->handle(req, res, params); */
                auto rules_params = method_rules_[(int)req.method].trie.find(req.url);
                if(rules_params.first == -1) {
                    log_info("can't found handler for url:", req.url);
                    return;
                }
                method_rules_[(int)req.method].rules[rules_params.first]->handle(req, res, rules_params.second);
            }
        private:
            void internal_add_rule_object(const std::string& rule, DynamicRule* rule_obj) {
                rule_obj->forearch_method([&](HttpMethod method) {
                    method_rules_[(int)method].rules.emplace_back(rule_obj);
                    method_rules_[(int)method].trie.add(rule, method_rules_[(int)method].rules.size() - 1);
                });
            }
        private:
            struct MethodRule
            {
                std::vector<DynamicRule*> rules;
                Trie trie;
            };
            std::vector<std::unique_ptr<DynamicRule>> all_rules_;
            std::array<MethodRule, (int)HttpMethod::METHOD_NUMS> method_rules_;
    };

}
