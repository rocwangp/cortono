#pragma once

#include "../util/util.hpp"

namespace cortono::net
{
    class Watcher
    {
        public:
            Watcher() {
                ::pipe(fd_);
            }

            ~Watcher() {
                util::io::close(fd_[0]);
                util::io::close(fd_[1]);
            }

            void notify() {
                ::write(fd_[1], "0", 1);
            }

            void clear() {
                char buffer[2];
                ::read(fd_[0], buffer, sizeof(buffer));
            }

            int read_fd() {
                return fd_[0];
            }

        private:
            int fd_[2];
    };
}
