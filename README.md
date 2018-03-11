# cortono

## 一个C++实现的Linux平台下简单网络库

* header only, 只包含头文件
* 使用c++11/14/17
* 支持半同步半异步(HSHA)模式
* 支持反应堆模式
* 带有一个简单的http服务器


### 使用示例

#### 回显服务器

```c
#include "/home/roc/unix/cortono/cortono.hpp"
using namespace cortono::net;

class echo_session : public tcp_session
{
    public:
        echo_session(std::shared_ptr<tcp_socket> socket)
            : tcp_session(socket)
        {
            set_read_callback(std::bind(&echo_session::handle_read, this));
        }

        void handle_read() {
            socket_->write(socket_->read_all());
        }
};

int main()
{
    event_loop base;
    tcp_service<echo_session> server{ &base, "localhost", 9999 };
    server.on_conn([&server](auto socket) {
        server.register_session(socket , std::make_shared<echo_session>(socket)); });
    server.start();
    base.sync_loop();
    return 0;
}
```

## 关于tcp_service::on_conn函数

有以下几点考虑

* 自定义的session_type的构造函数可能有多个参数，无法由内部网络库构造，需要在连接建立后显示构造
* 可以对socket进行更多详细的设置，如no_delay选项


## 不足及改进:

* 只是对网络请求的简单封装,主要是用于平时自己使用
* 在以后使用的过程中会根据新需求不断进行扩充

### 扩充计划有

* 完善HTTP服务器
* 支持UDP
* 支持心跳机制
* 支持protobuf
* 进行并发测试
