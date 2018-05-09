#include "../../../cortono.hpp"
#include "module.hpp"
#include "recv_module.hpp"
#include "send_module.hpp"
#include "resend_module.hpp"

template <std::uint64_t N, std::uint64_t M>
struct Power
{
    static const std::uint64_t value = N * Power<N, M - 1>::value;
};

template <std::uint64_t N>
struct Power<N, 0>
{
    static const std::uint64_t value = 1;
};

int main()
{
    using namespace cortono;
    constexpr std::uint64_t BufferSize = Power<2, 16>::value;
    constexpr std::uint64_t WindowSize = 200;

    cortono::net::EventLoop loop;
    cortono::net::UdpService<Connection<RecvModule<BufferSize, WindowSize>,
                                        SendModule<BufferSize, WindowSize>,
                                        ResendModule<BufferSize, WindowSize>
                                        >> service(&loop, "127.0.0.1", 10000);
    service.on_read([](auto conn_ptr) {

    });
    loop.loop();

    return 0;
}

