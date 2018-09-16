#include "../ip/sockets.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    auto results = cortono::ip::address::interface_address();
    std::copy(results.begin(), results.end(), std::ostream_iterator<std::string>{ std::cout, "\n" });
    return 0;
}
