#include <string>
#include <vector>
#include <iostream>

int main()
{
    std::string ip{ "127.0.0.1" };
    char* str_end = nullptr;
    const char* str_begin = ip.data();
    std::uint16_t n = std::strtol(str_begin, &str_end, 10);
    while(str_begin != str_end) {
        std::cout << n << std::endl;
        str_begin = str_end + 1;
        n = std::strtoul(str_begin, &str_end, 10);
    }
    return 0;
}

