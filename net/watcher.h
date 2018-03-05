#pragma once

#include <unistd.h>
#include "../util/util.h"

namespace cortono::net
{
    class watcher
    {
        public:
            watcher() {
                ::pipe(fd_);
            }

            ~watcher() {
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
