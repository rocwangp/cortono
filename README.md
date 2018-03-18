# cortono

## 一个C++实现的玩具型网络库

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
int main() {
    EventLoop base;
    TcpService service(&base, "127.0.0.1", 9999);
    service.on_message([](auto conn) { conn->send(conn->recv_all()); });
    service.start();
    base.loop();
    return 0;
}
```

## 不足及改进:

* 只是对网络请求的简单封装,主要是用于平时自己使用
* 在以后使用的过程中会根据新需求不断进行扩充

### 扩充计划有

* 完善HTTP服务器
* 支持UDP
* 支持心跳机制
* 支持protobuf
* 进行并发测试
