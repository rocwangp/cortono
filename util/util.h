#pragma once

#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

#include "noncopyable.h"

namespace cortono
{

#define log_trace   cortono::util::logger(cortono::util::logger::trace, __FILE__, __func__, __LINE__)
#define log_debug   cortono::util::logger(cortono::util::logger::debug, __FILE__, __func__, __LINE__)
#define log_info    cortono::util::logger(cortono::util::logger::info,  __FILE__, __func__, __LINE__)
#define log_error   cortono::util::logger(cortono::util::logger::error, __FILE__, __func__, __LINE__)
#define log_fatal   cortono::util::logger(cortono::util::logger::fatal, __FILE__, __func__, __LINE__)

    namespace util
    {

        template <typename T = int>
        std::string current_time() {
            auto t = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(t);
            char *tc = std::ctime(&tt);
            tc[std::strlen(tc) - 1] = '\0';
            return std::string(tc);
        }

        template <typename T = int>
        std::string current_thread() {
            std::stringstream oss;
            oss << std::this_thread::get_id();
            return oss.str();
        }

        class io
        {
            public:
                static int open(const std::string& filename) {
                    return ::open(filename.c_str(), O_RDONLY);
                }

                static void close(int fd) {
                    ::close(fd);
                }
        };

        class logger : private util::noncopyable
        {
            public:
                enum level
                {
                    trace,
                    debug,
                    info,
                    error,
                    fatal
                };
            public:
                logger(level l, const std::string& file, const std::string& func, int line)
                    : l_(l)
                {
                    buffer_ << util::current_time()
                            << " [" << util::current_thread() << "]"
                            << " [" << file << ":" << func << ":" << line << "]"
                            << " [" << format_level() << "] ";
                }

                ~logger() {
                    buffer_ << std::endl;
                    auto msg(buffer_.str());
                    std::fwrite(msg.c_str(), 1, msg.size(), ::stdout);
                    std::fflush(::stdout);
                    if(l_ == level::fatal)
                        ::abort();
                }

                template <typename T>
                logger& operator()(T msg) {
                    buffer_ << msg;
                    return *this;
                }

                template <typename T, typename... Args>
                logger& operator()(T msg, Args... args) {
                    buffer_ << msg << " ";
                    return operator()(args...);
                }


           private:
                std::string format_level()
                {
                    switch(l_) {
                        case level::trace:
                            return "trace";
                        case level::debug:
                            return "debug";
                        case level::info:
                            return "info ";
                        case level::error:
                            return "error";
                        case level::fatal:
                            return "fatal";
                        default:
                            return "";
                    }
                }


            private:
                level l_;
                std::stringstream buffer_;
        };



        template <class... Args>
        static std::string format(const std::string& f, Args... args) {
            char buffer[1024];
            std::sprintf(buffer, f.c_str(), std::forward<Args>(args)...);
            return std::string(buffer);
        }

        template <typename... Args>
        void exitif(bool condition, Args... args) {
            if(condition) {
                log_fatal(std::strerror(errno), args...);
            }
        }

        template <typename Function, typename... Args>
        auto invoke_if(Function&& f, Args... args, bool condition)
            -> std::pair<bool, typename std::invoke_result<Function>::type> {
            using result_type = typename std::invoke_result<Function>::type;
            if(condition) {
                return {true, f(std::forward<Args>(args)...)};
            }
            return {false, result_type{}};
        }
    }

}

