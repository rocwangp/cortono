# Web服务器的设计

整个服务器的后端框架都是参考[Crow](https://github.com/ipkn/crow)实现，也是为了熟悉相关的设计技巧以及模板元编程的应用方法



## 路由函数类型的设定

以HTTP服务器为例，首先确定http路由回调函数的参数类型，有几点考虑

- 函数接收一个HTTP请求对象Request和一个字符串流stringstream，用户可以从Request中获取请求信息，将想要返回给浏览器的整个HTTP响应数据写入字符串流，服务器后端从字符串流中提取数据传给浏览器
- 函数接收一个HTTP请求对象Request，用户将想要返回的响应体部分的数据通过字符串返回，服务器后端构造HTTP响应对象Response，构造HTTP响应数据，返回给浏览器
- 函数接收一个HTTP请求对象Request和一个响应对象Response（引用），用户直接对Response进行操作，服务器后端通过Response构造HTTP响应数据，返回给浏览器
- 函数什么数据也不接收，只返回用户规定的特定数据
- 函数可以接收任意多个某些特定类型的数据，实现用户特定功能





Crow整合了后四种的参数传入方式，即支持如下调用

```c++
// 什么也不接收，返回用户设置的信息，通常是默认页面
app.register_rule("/")([]() {
    return "hello world";
});
// 接收一个HTTP请求对象，可以获取请求信息
app.register_rule("/info")([](const Request& req) {
    std::string s;
    // 从req中获取请求信息，写入到s中
    return s;
});
// 接收两个int类型的变量，返回二者的和
// 浏览器访问时输入ip:port/adder/1/2     页面返回3
app.register_rule("/adder/<int>/<int>")([](int a, int b) {
    std::stringstream oss;
    oss << a + b;
    return oss.str();
});
// 接收一个一个请求对象，一个响应对象，一个路径（默认是string类型）
app.register_rule("/web/<path>")([](const Request& req, Response& res, std::string path) {
    ...
});
```

整理后发现，实际的函数类型有以下几种

```c++
// ...表示int64_t, uint64_t, double, string四种类型的任意组合，每种类型可以出现0次或多次
std::function<std::string(...)>
std::function<std::string(const Request&, ...)>
std::function<std::string(const Request&, Response&, ...)>
```

首先要做的就是将这三种不同类型的函数对象整合成统一类型，方法是再进行一次包装，假设用户传入的函数对象是f，二次封装后的函数对象是handler，另handler的参数为最多的那个，但是返回值为空

```c
std::function<void(const Request&, Response&, ...)> handler;
```

为了易于表示参数后面的...，采用另一个对象保存这些参数

```c++
struct routing_params
{
    std::vector<int64_t> int_params;
    std::vector<uint64_t> uint_params;
    std::vector<double> double_params;
    std::vector<std::string> string_params;
};
```

所以hanlder类型可以表示成

```c++
std::function<void(const Request&, Response&, const routing_params&)> handler;
```

## 萃取函数的参数类型

至此，当程序解析完HTTP请求信息后准备传参给用户执行函数时，只需要传入固定类型给handler即可（routing_params的构造会在最后提及）

现在handler拥有用于想要或者不想要的所有参数数据，接下来的首要任务是当进入到handler后如何将用户想要的哪些参数传过去，这里涉及到模板元编程的技巧

获取函数的返回值，参数个数以及参数类型

由于模板强大的特化能力，使得程序可以萃取任何想要的数据，而实际上一个函数要么是函数指针，要么是函数对象（内部的operator()重载），二者都有相同的特点就是都可以采用类似R(Args...)的这种形式表示，其中R是返回值类型，Args...是参数列表



```c++
template <typename T>
struct function_traits;

// 对函数指针的特化
template <typename R, typename... Args>
struct function_traits<R(Args...)>
{
    using return_type = R;
    static const std::size_t arg_size = sizeof...(Args);

    template <std::size_t N>
    using arg_type = std::tuple_element_t<N, std::tuple<Args...>>;
};

// 对const函数指针的特化
template <typename R, typename... Args>
struct function_traits<R(Args...) const>
{
    using return_type = R;
    static const std::size_t arg_size = sizeof...(Args);

    template <std::size_t N>
    using arg_type = std::tuple_element_t<N, std::tuple<Args...>>;
};

// 对函数对象的特化
template <typename ClassType, typename R, typename... Args>
struct function_traits<R(ClassType::*)(Args...)>
{
    using return_type = R;
    static const std::size_t arg_size = sizeof...(Args);

    template <std::size_t N>
    using arg_type = std::tuple_element_t<N, std::tuple<Args...>>;
};

// 对const函数对象的特化
template <typename ClassType, typename R, typename... Args>
struct function_traits<R(ClassType::*)(Args...) const>
{
    using return_type = R;
    static const std::size_t arg_size = sizeof...(Args);

    template <std::size_t N>
    using arg_type = std::tuple_element_t<N, std::tuple<Args...>>;
};

// 函数对象无法写成R<Args...>的形式，而函数对象重载的operator()函数实际上是函数类型，可以写成R<Args...>的形式
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())>
{
};
```

## 路由函数的二次包装

函数萃取的主要目的是获取每个参数的类型，接下来就可以进行特定的类型绑定了，回想一下handler的类型

```c++
std::function<void(const Request&, Response&, const routing_params&)> handler0;
```

不过再次之前先把它还原成原来的样子，即

```c++
std::function<void(const Request&, Response&, Args...)> handler;
```

实际上这是两个handler，作用在不同的作用域下（注意为了区分已经将第一个handler改名成handler0）



对于上述原始的三种函数类型，依次对应的handler实现如下，由于这里面使用的都是Args...，所以对于handler0的赋值实际上是不合法的，所以要更改表示方法

```c++
// 原始类型是std::function<std::string(const routing_params&)>;
// 也就是Args... 等价于 const routing_params
// std::function<void(const Request&, Response&, Args...)>与目标类型std::function<void(const Request&, Response&, const routing_params&)>匹配
template <typename Func typename... Args>
void set_(Func f) {
	handler = [f = std::move(f)](const Request&, Response& res, Args... args) {
		res = Response(f(args...));
	};
}
```


```c++
// 原始类型是std::function<std::string(const Request&, const routing_params&)>
// 也就是参数Args... 包括 const Request& 和 const routing_params&
// 需要进一步萃取，将Args...中的const Request&抽取出来
template <typename Func, typename... Args>
void set_(Func f) {
    handler = req_handler_wrapper<Func, Args...>(std::move(f));
}

// 这里将Req萃取出来，Args...只剩下const routing_params&部分
// 此时std::function<void(const Request&, Response&, Args...)>于目标类型匹配
template <typename Func, typename Req, typename... Args>
struct req_handler_wrapper
{
    req_handler_wrapper(Func func) : f(std::move(func)) {}
    
    // 有operator()的类可以隐式转换成std::function<...>对象
    void operator()(const Request& req, Response& res, Args... args) {
        res = Response(f(req, args...));
    }
    Func f;
};
```


```c++
// 原始类型是std::function<std::string(const Request&, Response&, const routing_params&)>
// 也就是参数Args... 包括 const Request&, Response&, const routing_params&
// 此时std::function<void(Args...)>于目标类型匹配
template <typename Func, typename Args...>
void set_(Func f) {
    handler = std::move(f);
}
```



实际上handler0只是handler的又一层包装，handler仅仅作用在一个函数对象中，因为C++允许一个重载了operator()函数的类隐式转换成std::funciton<...>对象，这里可以参考源码[http_router.hpp](https://github.com/rocwangp/cortono/blob/master/http/http_router.hpp#L118)

最后的任务就是传参问题了，在后端的解析过程中，已经将所有值类型的参数保存在routing_params中了，现在的问题是如何根据目标参数列表将routing_params中的值按照类型依次填到形参中，方法是根据当前特点类型进行特化，分别特化出int64_t，uint64_t，double，string四种版本，同时采用4个int类型的模板参数记录每个类型是当前第几个遇到的（实际上就是在type_params中的下标），完整代码可以参考[http_router.hpp](https://github.com/rocwangp/cortono/blob/master/http/http_router.hpp#L172)

