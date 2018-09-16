#include "../session.hpp"
#include "../peer.hpp"
#include "../service.hpp"

#include <iostream>
int main()
{
    std::cout << typeid(typename p2p::SessionManager<p2p::Service>::parent_t::item_t).name() << std::endl;
    std::cout << typeid(typename p2p::PeerManager::parent_t::item_t).name() << std::endl;
    return 0;
}
