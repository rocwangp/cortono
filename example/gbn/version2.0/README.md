# 基本流程

在重构代码的过程中，考虑到扩展性，决定在框架层面不添加任何功能相关的代码，整个滑动窗口体系的设计由使用者自行设计。但是又考虑到整个体系可能覆盖的内容较多，所以改用模块的思想实现。

## 五种报文段类型

底层框架仅仅暴露数据报文段格式MsgPacket类型，用户自己实现想要携带的信息即可。并将报文段分成五类，分别是

* 错误报文段
* 收到的数据报文段
* 收到的确认报文段
* 发送的数据报文段
* 发送的确认报文段

## 模块的引入

此外，借鉴Nginx的设计思想，采用模块化的方法为框架提供功能支持。以接受报文段为例，程序的执行流程为

* 接受数据
* 解析数据，构造报文段MsgPacket对象
* 依次调用各模块的check方法，判断收到的报文段是否合法，只要有一个返回false，则终止此次流程
* 依次调用各模块的handle方法，对报文段对象进行修改，实现想要的功能
* 判断此时的报文段是否需要发送，如需发送则执行发送操作，否则返回
* 如果上述操作不存在错误，则调用用户注册的可读回调函数



为了满足上述流程，要求用户提供的模块需要至少提供check和handle方法，接口类型如下

```c++
// Packet&既是收到的报文段对象，又是将要发送出去的报文段对象（如果需要）
// 每个模块既可以从packet中获取接受到的信息，也可以更改packet中的信息
//（如接受模块可能需要设置应答的ack数值）

// 在执行handle前会调用每个模块的check方法
//一旦某个模块的check返回false，证明收到的报文段不符合要求，终止本次流程
template <typename Packet>
bool check(Packet& packet) {
    
}

// 所有模块的check方法执行完成且没有出错后，依次调用各模块的handle方法
// handle方法用于对packet进行修改
template <typename Packet>
bool handle(Packet& packet) {
    
}
```

模块设计者可以自定义上述接口的实现，但通常推荐以下面的格式实现接口

```c++
template <typename Packet>
bool check(Packet& packet) {
    if(packet.is_error_packet()) {

    }
    else if(packet.is_recv_data_packet()) {

    }
    else if(packet.is_recv_ack_packet()) {

    }
    else if(packet.is_send_data_packet()) {

    }
    else if(packet.is_ack_data_packet()) {

    }
    else {
        // error
    }
}

template <typename Packet>
bool handle(Packet& packet) {
    if(packet.is_error_packet()) {

    }
    else if(packet.is_recv_data_packet()) {

    }
    else if(packet.is_recv_ack_packet()) {

    }
    else if(packet.is_send_data_packet()) {

    }
    else if(packet.is_send_ack_packet()) {

    }
    else {
        // error
    }
}
```

## 接口的实现需要遵循规则

正如最开始所述，程序将报文段分为5种类型，各模块的相应接口可以根据当前报文段的类型决定执行哪些操作。但是需要注意也是非常重要的一点是，对报文段的修改不能触及三个信息，分别是

* 报文段的源ip，源端口
* 报文段的目的ip，目的端口
* 报文段的控制位

原因是在解析模块中，是通过上述三个信息来判断当前报文段的类型的，所以一旦前面的报文段修改某个信息，就会导致后面的模块判断报文段类型时出错



为了解决这个看似麻烦的问题，推荐使用者仅仅在最后一个模块完成对上述三条信息的修改，也就是单独引入一个ModifyModule模块，负责修改相关信息

在实现各模块的相关接口时，最好遵循一个标准，即

> 每个模块只需要修改只有当前模块能够提供的信息即可



# 各模块的初始化

框架需要各模块的构造函数接受相同的参数，分别是

* 当前的事件循环指针cortono::net::EventLoop*
* 本地ip地址
* 本地端口号

之所以是这三个内容，是因为在模块的运行过程中几乎不会依靠外部数据

* 所需的报文段是通过参数传递
* ip地址和端口号也仅仅是最后一个模块对报文段敏感的三条信息做更改时才会使用
* 事件循环是用于为重传模块的定时器实现提供方便


# 基础模块简介

为了实现基本的滑动窗口协议，框架内部提供了相应的模块实现，分别是

* 解析模块，负责解析报文段
* 丢包模块，负责模拟丢包
* 接受模块，负责接受窗口的维护以及数据的接受和存储
* 发送模块，负责发送窗口的维护
* 重传模块，负责超时重传
* 修改模块，负责在最后修改报文段的敏感信息


## 解析模块

解析模块是唯一一个框架内部依赖的模块，也是唯一内置的模块。该模块负责的主要任务是

* 将接收到的数据解析成数据报对象
* 提供接口返回当前数据报的类型（开篇5种）
* 提供接口返回相应数据，如获取序列号，确认号等
* 提供接口用于修改相应的数据

虽然解析模块由框架内置，但是仍然向用户暴露实现细节，目的是便于用户为数据报增加新的字段以满足新增模块的需求，如通告窗口，校验和等

模块的初始化接受六个参数，三对(ip, port)，分别是

* 源ip，源port，记作src_ip, src_port
* 目的ip，目的port，记作des_ip, des_port
* 本机ip，本机port，记作local_ip, local_ip

保存三对ip和port的目的是为了判断当前报文段类型

在接受数据报文段时，源表示发送方，目的与本机相同，所以判断方法为

```c++
bool is_recv_ack_packet() const {
	return !is_error_packet() && local_ip_ == des_ip_ && local_port_ == des_port_ && control_[1] == '1';
}
bool is_recv_data_packet() const {
	return !is_error_packet() && local_ip_ == des_ip_ && local_port_ == des_port_ && control_[1] == '0';
}
```

在发送数据报文段时，源与本机相同，目的表示接收方，所以判断方法为

```c++
bool is_send_ack_packet() const {
	return !is_error_packet() && (local_ip_ != des_ip_ || local_port_ != des_port_) && control_[1] == '1';
}
bool is_send_data_packet() const {
	return !is_error_packet() && (local_ip_ != des_ip_ || local_port_ != des_port_) && control_[1] == '0';
}
```

> control是6个控制位，第1个表示ACK标志

其中，判断当前报文段是否出错仅仅是检查源和目的是否相同，这也为丢包模块的实现提供了思路

```c++
bool is_error_packet() const {
	return src_ip_ == des_ip_ && src_port_ == des_port_;
}
```



## 丢包模块

为了模拟丢包，框架提供了丢包模块，该模块需要运行在任何实际功能模块之前。实现思路很简单，随机判断是否需要模拟丢包，如果需要，则将收到的报文段类型修改成错误类型，这样接下来其他模块的handle函数中判断packet类型时会发现是一个错误类型的数据报，什么都不会做

令一个数据报出错，只需要令源和目的相同即可

> 丢包模块也是除了最后一个模块外唯一可以更改数据报的ip和port的模块

实现起来比较简单

```c++
template <typename Parser>
bool handle(Parser& parser) {
    if(parser.is_recv_data_packet()) {
        if(std::rand() % 60000 < 60000 * Num / Denom) {
        // if the src_ip == des_ip and the src_port == des_port
        // the packet will be seen as the error packet
        parser.set_error_packet();
        }
    }
    return true;
}
```



## 接收模块

该模块仅在接收到数据报时执行相关操作

凡是有数据交互需求的程序都需要实现接受模块，所以框架仍然内置了一个，该模块需要维护一个接受窗口。因为在正常的滑动窗口协议工作流程中（指SR），一旦收到对端发送的数据并且序列号范围在接受窗口的有效范围内，就设置接受标识并保存这部分数据，同时根据接收到的序列号是否是接受窗口的左边界来决定是否移动窗口。滑动窗口相关的内容可以参考[TCP/IP学习笔记（三）TCP流量控制以及滑动窗口](https://rocwangp.github.io/2018/02/24/TCP-IP%E5%8D%8F%E8%AE%AE%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0%EF%BC%88%E4%B8%89%EF%BC%89TCP%E6%B5%81%E9%87%8F%E6%8E%A7%E5%88%B6%E4%BB%A5%E5%8F%8A%E6%BB%91%E5%8A%A8%E7%AA%97%E5%8F%A3/#more)

模块负责的工作有

* 保存发送方的ip和port，方便上层的回调函数获取
* 如果序列号在有效范围内则设置接受窗口相应的标志位并保存数据
* 尝试移动接受窗口
* 设置确认号为接受窗口的左边界

这里可以看到对修改标准的遵循，由于确认号只能接受模块提供，所以该模块必须也只需修改报文段的确认号字段



## 发送模块

该模块维护了一个发送窗口，只有在发送窗口范围内的序列号才允许发送，而当收到应答报文段时会尝试移动发送窗口以便发送更多的数据

当主动发送数据时，用户可以通过解析模块构造发送报文段，等到发送模块时，由于只有该模块知道发送窗口的布局以及应该为这段数据提供多大的序列号，所以发送模块会在判断是发送数据时修改报文段的序列号字段



## 重传模块

该模块和发送模块是一一对应的，当发送数据时，设置定时器，当接受数据时，取消定时器

但是在这个模块实现时发现由于只有框架才知道如何发送数据，导致外部模块无法直接调用发送api进行发送，解决办法是框架初始化过程中为每个需要发送方法的模块提供发送方法，如果某个模块需要发送数据，只需要实现一个名为bind_sender的接口即可，框架在初始化时会判断传入的每个模块是否具有名为bind_sender的接口，如果有则传入发送方法



## 修改模块

该模块主要做一些收尾工作，如果收到的是数据报文段，则修改报文段将其变为发送ack类型的报文段，由于接受模块已经设置了确认号，所以该模块主要修改ip和端口，以及控制位相关的信息

