#include "../entry.hpp"
#include <iostream>

int main()
{
    p2p::AddrDatagram datagram{ "127.0.0.1", 8090 };
    std::string bytes = datagram.serialize();
    p2p::Datagram::parse_datagram_header(bytes);

    p2p::GetAddrDatagram datagram2{};
    bytes = datagram2.serialize();
    p2p::Datagram::parse_datagram_header(bytes);
    return 0;
}
