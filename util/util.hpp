#pragma once

#include "../std.hpp"
#include "noncopyable.hpp"
#include <experimental/filesystem>

namespace cortono
{

#define log_trace   cortono::util::logger(cortono::util::logger::trace, __FILE__, __func__, __LINE__)
#define log_debug   cortono::util::logger(cortono::util::logger::debug, __FILE__, __func__, __LINE__)
#define log_info    cortono::util::logger(cortono::util::logger::info,  __FILE__, __func__, __LINE__)
#define log_error   cortono::util::logger(cortono::util::logger::error, __FILE__, __func__, __LINE__)
#define log_fatal   cortono::util::logger(cortono::util::logger::fatal, __FILE__, __func__, __LINE__)

    namespace util
    {

        inline std::string current_time() {
            auto t = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(t);
            char *tc = std::ctime(&tt);
            tc[std::strlen(tc) - 1] = '\0';
            return std::string(tc);
        }

        inline std::string current_thread() {
            try{
                std::stringstream oss;
                oss << std::this_thread::get_id();
                return oss.str();
            }
            catch(...) {
                return "";
            }
        }

        class exitcall
        {
            public:
                typedef std::function<void()> callback;

                exitcall(callback cb)
                    : cb_(std::move(cb))
                {  }

                ~exitcall() {
                    if(cb_) {
                        cb_();
                    }
                }
            private:
                callback cb_;
        };
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
                    if(is_open) {
                        buffer_ << util::current_time()
                                << " [" << util::current_thread() << "]"
                                << " [" << format_level() << "]"
                                << " [" << file << ":" << func << ":" << line << "] ";
                    }
                }

                ~logger() {
                    if(!is_open) {
                        return;
                    }
                    buffer_ << std::endl;
                    auto msg(buffer_.str());
                    std::fwrite(msg.c_str(), 1, msg.size(), ::stdout);
                    std::fflush(::stdout);
                    if(l_ == level::fatal)
                        ::abort();
                }

                template <typename T, typename... Args>
                logger& operator()(T msg, Args... args) {
                    if(!is_open) {
                        return *this;
                    }
                    buffer_ << msg;
                    if constexpr(sizeof...(Args) == 0) {
                        return *this;
                    }
                    else {
                        return operator()(args...);
                    }
                }

                template <typename... Args>
                logger& operator<<(Args&&... args) {
                    return operator()(std::forward<Args>(args)...);
                }

                static void close_logger() {
                    is_open = false;
                }

           private:
                std::string format_level()
                {
                    switch(l_) {
                        case level::trace:
                            return "Trace";
                        case level::debug:
                            return "Debug";
                        case level::info:
                            return "Info";
                        case level::error:
                            return "Error";
                        case level::fatal:
                            return "Fatal";
                        default:
                            return "    ";
                    }
                }


            private:
                level l_;
                std::stringstream buffer_;

                static bool is_open;
        };
        inline bool logger::is_open = true;



        template <class... Args>
        static std::string format(const std::string& f, Args&&... args) {
            char buffer[1024];
            std::sprintf(buffer, f.c_str(), std::forward<Args>(args)...);
            return std::string(buffer);
        }

        template <typename... Args>
        void exitif_impl(bool condition, const std::string& file, std::size_t line, const std::string&& func, Args... args) {
            if(condition) {
                log_fatal(std::strerror(errno), file, func, line, args...);
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

        inline auto from_chars(const std::string& str) {
            return std::strtol(str.c_str(), nullptr, 10);
        }

        template <typename T>
        inline auto to_chars(const T& value) {
            std::stringstream oss;
            oss << value;
            return oss.str();
        }

        inline auto to_lower(std::string_view s) {
            std::string ls;
            ls.reserve(s.length());
            for(auto &ch : s) {
                ls.append(1, std::tolower(ch));
            }
            return ls;
        }

        inline std::size_t get_filesize(std::string_view filename) {
            using namespace std::experimental;
            filesystem::path p{ filename };
            if(!filesystem::exists(p)) {
                return 0;
            }
            else {
                return filesystem::file_size(p);
            }
        }

        inline std::vector<std::string_view> split(std::string_view str, char delimiter) {
            std::vector<std::string_view> results;
            std::size_t front = 0, back = 0;
            while(back <= str.length()) {
                if(back == str.length() || str[back] == delimiter) {
                    results.emplace_back(str.substr(front, back - front));
                    front = back + 1;
                }
                ++back;
            }
            return results;
        }
    }

#define exitif(condition, ...) \
    util::exitif_impl(condition, __FILE__, __LINE__, __func__, __VA_ARGS__)
}

