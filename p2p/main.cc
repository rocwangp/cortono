#include "service.hpp"
#include "configuration.hpp"

int main()
{
    p2p::Configuration configuration("conf.xml");
    p2p::Service{configuration}.run();
    
    return 0;
}
