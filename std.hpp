#pragma once

#include <fstream>
#include <vector>
#include <list>
#include <queue>
#include <string>
#include <string_view>
#include <set>
#include <map>
#include <unordered_map>

#include <iterator>
#include <type_traits>
#include <algorithm>
#include <atomic>
#include <memory>
#include <functional>

#include <regex>
#include <chrono>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>

#include <sstream>
#include <stdexcept>

#include <cstdint>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>
#include <sys/epoll.h>
#include <poll.h>

#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <zlib.h>

#include <experimental/filesystem>

#ifndef CORTONO_USE_SSL
#define CORTONO_USE_SSL
#endif

#ifdef CORTONO_USE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#define CA_CERT_FILE "../ssl/ca.crt"
#define SERVER_CERT_FILE "../ssl/server.crt"
#define SERVER_KEY_FILE "../ssl/server.key"
#define CLIENT_CERT_FILE "../ssl/client.crt"
#define CLIENT_KEY_FILE "../ssl/client.key"
#endif

