# 非阻塞connect的处理方法

除了可读/可写回调函数中可能存在的阻塞操作之外，网络框架的基本组件尽量不要出现阻塞调用，是提高并发性的基本要求

对于服务器相关操作而言，非阻塞监听套接字，非阻塞连接套接字已经满足上述要求

对于客户端相关操作，需要特别关注的一点是非阻塞connect的处理

正常情况下，阻塞connect要么连接成功，要么连接失败，而非阻塞connect多了一种正在连接（正在进行握手）的状态。

为什么需要非阻塞connect

当目标服务器可以正常连接时，非阻塞connect可以利用三次握手的时间执行其它操作

当目标服务器无法正常连接时，非阻塞connect会立即返回而阻塞connect会在超时时间到达后才放弃连接



## 如何处理非阻塞connect的返回结果

需要以下几个步骤

- 设置非阻塞套接字，开启对可读事件的监听，调用connect尝试连接到目标服务器
- 如果connect返回0，表示连接成功
- 如果connect返回-1，errno != EINPROGRESS，表示连接失败
- 如果connect返回-1，errno == EINPROGRESS，表示正在进行连接

## 根据套接字的状态判断连接结果

* 如果连接建立成功，则套接字会在连接完成后变为既可读又可写，同时通过getsockopt获取套接字错误码时返回0（表示没有错误发生）
* 如果连接建立失败，则套接字会在连接失败后变为可读



代码片段

```c++
// 尝试连接到目标服务器
void connect(EventLoop* loop, ip, port) {
    int fd = ip::tcp::sockets::nonblock_socket();
    auto conn = std::make_shared<TcpConnection>(loop, fd);
    if(!ip::tcp::sockets::connect(fd, ip, port)) {
        // 正在握手
        if(errno == EINPROGRESS) {
            conn->set_conn_state(TcpConnection::ConnState::HandShaking);
        }
        // 连接失败
        else {
            conn->set_conn_state(TcpConnection::ConnState::Closed);
        }
    }
    // 连接成功
    else {
        conn->set_conn_state(TcpConnection::ConnState::Connected);
    }

    // 设回调
    conn->on_read(...);
    conn->on_write(...);
    conn->on_close(...);
    
    ...
}

// conn的可读回调函数中
void handle_read() {
    if(conn_state_ == ConnState::HandShaking) {
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLOUT | POLLERR;
        if(poll(&pfd, 1, 0) == 1) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, sizeof(len));
            if(err == 0) {
                conn_state_ = ConnState::Connected;
            }
            else {
                conn_state_ = ConnState::Closed;
            }
        }
        else {
            conn_state_ = ConnState::Closed;
        }
    }
    ...// 执行读操作或者返回
}
```

------

完整代码参考[客户端连接服务器](https://github.com/rocwangp/cortono/blob/master/net/client.hpp)和[正在建立连接状态的处理](https://github.com/rocwangp/cortono/blob/master/net/connection.hpp)的handle_read/handle_handshake函数

