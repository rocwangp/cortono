#include "../unility.hpp"

#include <iostream>

int main(/* int argc, char *argv[] */)
{
    std::uint16_t port{ 8090 };
    std::cout << std::boolalpha << (p2p::string_to_number<std::uint16_t>(p2p::number_to_string(port).data(), 2)  == port) << std::endl;
    return 0;
}
