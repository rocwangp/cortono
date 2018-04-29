#include "http_client.hpp"

int main()
{
    using namespace cortono::http;
    HttpClient{}
        .ip("127.0.0.1")
        .port(8080)
        .url("/")
        .version(1, 1)
        .keep_alive(false)
        .connect();
    return 0;
}
