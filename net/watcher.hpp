#pragma once

#include "../util/util.hpp"

namespace cortono::net
{
    class Watcher
    {
        public:
            Watcher() {
                int ret = ::pipe(fd_);
                (void)ret;
            }

            ~Watcher() {
                util::io::close(fd_[0]);
                util::io::close(fd_[1]);
            }

            void notify() {
                int ret = ::write(fd_[1], "0", 1);
                (void)ret;
            }

            void clear() {
                char buffer[2];
                int ret = ::read(fd_[0], buffer, sizeof(buffer));
                (void)ret;
            }

            int read_fd() {
                return fd_[0];
            }

        private:
            int fd_[2];
    };
}
