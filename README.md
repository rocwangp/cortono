# cortono

## 采用C++11/14/17实现的网络库

* header only
* 支持半同步半异步模式
* 支持半同步半反应堆模式
* 带有一个web服务器，支持HTTPS，支持HTTP/HTTPS代理



## 平台支持

- Linux: ubuntu16 64bit gcc7.2.0 测试通过




## 编译依赖

* g++需要支持C++17
* openSSL，使用HTTPS服务器时依赖
* -std=c++17 用于支持C++17（使用网络库时需要）
* -lstdc++fs 用于支持std::experimental::filesystem（使用网络库时需要）
* -lpthread Linux下用于支持线程库（使用网络库时需要）
* -lssl -lcrypto 用于支持openSSL（使用HTTPS服务器时需要）




## 基本组件简介

采用多线程模型实现半同步半异步模式以及半同步半反应堆模式，具体请参考[多线程并发模型简介](https://github.com/rocwangp/cortono/blob/master/doc/%E5%A4%9A%E7%BA%BF%E7%A8%8B%E5%B9%B6%E5%8F%91%E6%A8%A1%E5%9E%8B%E7%AE%80%E4%BB%8B.md)

采用epoll接管连接，使用边缘触发结合应用层缓冲区的方式接受数据，具体请参考[epoll水平触发和边缘触发的选择](https://github.com/rocwangp/cortono/blob/master/doc/epoll%E6%B0%B4%E5%B9%B3%E8%A7%A6%E5%8F%91%E5%92%8C%E8%BE%B9%E7%BC%98%E8%A7%A6%E5%8F%91%E7%9A%84%E9%80%89%E6%8B%A9.md)

采用非阻塞套接字实现连接的接受与数据的读写，具体请参考[非阻塞监听套接字接收连接请求时的处理方法](https://github.com/rocwangp/cortono/blob/master/doc/%E9%9D%9E%E9%98%BB%E5%A1%9E%E7%9B%91%E5%90%AC%E5%A5%97%E6%8E%A5%E5%AD%97%E6%8E%A5%E6%94%B6%E8%BF%9E%E6%8E%A5%E8%AF%B7%E6%B1%82%E6%97%B6%E7%9A%84%E5%A4%84%E7%90%86%E6%96%B9%E6%B3%95.md)

客户端采用非阻塞connect减少连接阻塞时间，具体参考[非阻塞connect的处理方法](https://github.com/rocwangp/cortono/blob/master/doc/%E9%9D%9E%E9%98%BB%E5%A1%9Econnect%E7%9A%84%E5%A4%84%E7%90%86%E6%96%B9%E6%B3%95.md)

程序可以同时使用SSL和TCP，而不是只能使用其中一种，具体参考[兼容SSL的考虑](https://github.com/rocwangp/cortono/blob/master/doc/%E5%85%BC%E5%AE%B9SSL%E7%9A%84%E8%80%83%E8%99%91.md)

openSSL的使用方法请参考[OpenSSL的使用](https://github.com/rocwangp/cortono/blob/master/doc/OpenSSL%E7%9A%84%E4%BD%BF%E7%94%A8.md)

关于Web服务器的设计（借鉴crow）请参考[Web服务器的设计](https://github.com/rocwangp/cortono/blob/master/doc/Web%E6%9C%8D%E5%8A%A1%E5%99%A8%E7%9A%84%E8%AE%BE%E8%AE%A1.md)



## 使用示例

### 回显服务器

```c
#include <cortono/cortono.hpp>	
using namespace cortono::net;
int main() {
    EventLoop base;
    TcpService service(&base, "127.0.0.1", 9999);
    service.on_message([](auto conn) { conn->send(conn->recv_all()); });
    service.start();
    base.loop();
    return 0;
}
```

### 定时任务

```c++
#include <cortono/cortono.hpp>	
int main()
{
    cortono::net::EventLoop base;
    base.runAfter(std::chrono::milliseconds(1000), []{
        log_info("after timer expires");
    });
    base.runEvery(std::chrono::milliseconds(2000), []{
        log_info("every timer expires");
    });
    base.loop();
    return 0;
}
```

### HTTP(S)代理服务器

```c++
#include <cortono/http/app.hpp>
int main()
{
    using namespace cortono::http;
    SimpleApp app;
    app.bindaddr("127.0.0.1")
       .port(9999)
       .proxy_server()
       .multithread()
       .https()
       .run();
    return 0;
}
```



### HTTP(S)服务器

```c++
#include <cortono/http/app.hpp>
int main()
{
    cortono::http::SimpleApp app;
    app.register_rule("/")([]() {
        log_trace;
        return "hello world";
    });
    app.register_rule("/adder/<int>/<int>")([](int a, int b) {
        std::stringstream oss;
        oss << a + b;
        return oss.str();
    });
    app.register_rule("/info")([](const cortono::http::Request& req) {
        std::stringstream oss;
        oss << req.method_to_string() << " "
            << req.raw_url << " "
            << "HTTP/" << req.version.first << "." << req.version.second
            << "\r\n";
        for(auto& [key, value] : req.header_kv_pairs) {
            oss << key << ": " << value << "\r\n";
        }
        oss << "\r\n";
        oss << req.body;
        return oss.str();
    });
    app.register_rule("/web/<path>")([](const cortono::http::Request&, cortono::http::Response& res, std::string s) {
        res = cortono::http::Response(200);
        res.send_file("web/" + s);
    });

    app.multithread().port(10000).run();
    return 0;
}
```



### 扩充计划

* 支持UDP
* 支持心跳机制
* 支持protobuf
* 进行并发测试




## 参考

- chenshuo的[muduo](https://github.com/chenshuo/muduo)
- yedf的[handy](https://github.com/yedf/handy)


- ipkn的[crow](https://github.com/ipkn/crow)

非常感谢三位！
