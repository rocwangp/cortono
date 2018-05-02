# OpenSSL的使用

HTTP协议建立在TCP协议之上，主要用在网页的数据传输上，客户端与服务器之间的数据采用明文传输，所以极易被中间人劫持，造成信息泄露

HTTPS意在解决HTTP明文传输的缺陷，在应用层与传输层之间增加了一层SSL协议（安全套接字协议），在传输数据之前进行加密，接收数据后进行解密

OpenSSL内置了大量加密算法，提供了很多接口用于实现应用层的数据交互，使用步骤如下

## 初始化SSL

```c
::SSLeay_add_ssl_algorithms();
::OpenSSL_add_all_algorithms();
::SSL_load_error_strings();
::ERR_load_BIO_strings();
::SSL_library_init();
```

## 创建会话环境SSL_CTX

```c
::SSL_CTX* ssl_ctx = ::SSL_CTX_new(::SSLv23_method());
```

## 加载相关证书，私钥

服务器加载CA证书，加载自己的证书，加载自己的私钥，验证私钥和证书是否相符（客户端不需要加载）

```c
#define CA_CERT_FILE "ssl/ca.crt"
#define SERVER_CERT_FILE "ssl/server.crt"
#define SERVER_KEY_FILE "ssl/server.key"

SSL_CTX_load_verify_locations(ssl_ctx, CA_CERT_FILE, nullptr);
SSL_CTX_use_certificate_file(ssl_ctx, SERVER_CERT_FILE, SSL_FILETYPE_PEM);
SSL_CTX_use_PrivateKey_file(ssl_ctx, SERVER_KEY_FILE, SSL_FILETYPE_PEM);
SSL_CTX_check_private_key(ssl_ctx);
```

## 建立连接

当需要建立连接时，首先调用TCP相关接口接收到连接套接字fd，然后创建和绑定SSL对象

```c
struct sockaddr_in sockaddr;
bzero(&sockaddr, sizeof(sockaddr));
socklen_t len = sizeof(sockaddr);
int fd = accept(listen_fd_, (struct sockaddr*)&sockaddr, &len);
SSL* ssl = SSL_new(ssl_ctx);
SSL_set_fd(ssl, fd);
```



## 处理读写事件

```c
char buffer[1024] = "\0";
SSL_read(ssl, buffer, sizeof(buffer));

SSL_write(ssl, buffer, sizeof(buffer));
```



## 关闭连接

```c
SSL_shutdown(ssl);
SSL_free(ssl);
```



## 程序结束前释放SSL会话环境SSL_CTX

```c
SSL_CTX_free(ssl_ctx);
```

------

完整代码封装[参考这里](https://github.com/rocwangp/cortono/blob/master/ip/sockets.hpp#L191)

