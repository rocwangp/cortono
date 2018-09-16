#include "../configuration.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    p2p::Configuration configuration("../conf.xml");
    std::cout << configuration.local_ip << std::endl;
    std::cout << configuration.local_port << std::endl;
    for(const auto& seed_node : configuration.seed_nodes) {
        std::cout << seed_node.ip() << ":" << seed_node.port() << std::endl;
    }
    return 0;
}
