# 兼容SSL的考虑

由于之前的网络框架都只集中在对TCP连接的封装，所以当遇到需要兼容SSL的时候就需要对程序做些修改

在修改初期，只是通过宏定义来有选择的执行TCP操作或SSL操作，如对于读取数据的操作

```c
#ifdef CORTONO_USE_SSL
	ip::tcp::ssl::recv(ssl_, buffer, bytes);
#else
	ip::tcp::socket::recv(fd_, buffer, bytes);
#endif
```

但是在后期使用时发现一旦程序运行就代表着只能使用TCP或者SSL中的一个，无法满足同时使用二者的需求，所以决定重新设计框架逻辑

## 整合SSL与TCP连接

仔细观察后发现，TCP和SSL实际上只是几个api的不同，而代表客户端与服务器连接的类结构实际上可以与二者无关，也就是定义如下结构

```c++
template <typename Socket>
class Connection : public std::enable_shared_from_this<Socket>
{
	public:
    	using socket_t = Socket;
    	...
    private:
    	socket_t socket_;
    	...
};
```

如此，程序只需要分别为TCP和SSL定义处理对应api的Socket即可，使用方法如下

```c++
using TcpConnection = Connection<TcpSocket>;
using SslConnection = Connection<SslSocket>;
```

## 重新实现TcpSocket和SslSocket

所以接下来的主要任务是实现TcpSocket以及SslSocket，而TcpSocket直接沿用之前的定义即可，无需变化。对于SslSocket，它实际上是在TCP上封装了一层SSL，也就是除了需要的套接字fd之外，还需要ssl，那么完全可以令SslSocket继承自TcpSocket，然后增加ssl成员变量

同时，TCP和SSL实际上也只是读写api的不同，那么就可以直接在SslSocket中隐藏掉TcpSocket的同名接口重新实现（这里无需采用虚函数的覆盖，因为没有多态的使用需求）

```c++
class TcpSocket
{
	public:
    	TcpSocket(int fd) : fd_(fd) {}
    	...
    	
    	void send(const char* buffer, int len);
    	void recv(char* buffer, int len);
    private:
    	int fd_;
    	...
};

class SslSocket : public TcpSocket
{
	public:
    	SslSocket(int fd, SSL* ssl) : TcpSocket(fd), ssl_(ssl) {}
    	...
    	
    	void send(const char* buffer, int len);
    	void recv(char* buffer, int len);
    private:
    	SSL* ssl_;
    	...
};
```

## 解决构造函数传参问题

然而到这里又遇到一个问题，那就是Connection中Socket的构造问题，由于TcpSocket构造函数需要一个参数而SslSocket需要两个，无法用正常的传参解决，分析之后决定采用可变参数模板实现

```c++
template <typename Socket>
class Connection : public std::enable_shared_from_this<Socket>
{
	public:
    	using socket_t = Socket;
    	
    	template <typename... Args>
    	Connection(EventLoop* loop, Args... args)
    		: loop_(loop),
    		  socket_(args...)
    	{ }
    	...
    private:
    	EventLoop* loop_;
    	socket_t socket_;
    	...
};
```

------

至此Connection设计完成，其中大部分组件沿用之前的实现，完整代码参考[connection.hpp](https://github.com/rocwangp/cortono/blob/master/net/connection.hpp)，[socket.hpp](https://github.com/rocwangp/cortono/blob/master/net/socket.hpp)以及[ssl_socket.hpp](https://github.com/rocwangp/cortono/blob/master/net/ssl_socket.hpp)

