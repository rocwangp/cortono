#pragma once

#include <string>
#include <iostream>

namespace p2p {

template <typename T>
T string_to_number(const char* front) {
    return *reinterpret_cast<const T*>(front);
}

template <typename T>
std::string number_to_string(T value) {
    return std::string{ reinterpret_cast<char*>(&value), sizeof(T) };
}
 
}
