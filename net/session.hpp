#pragma once

#include <memory>
#include "socket.hpp"
#include "../util/noncopyable.hpp"

namespace cortono::net
{
    class cort_session : public std::enable_shared_from_this<cort_session>,
                         private util::noncopyable
    {
        public:
            cort_session(std::shared_ptr<cort_socket> socket)
                : socket_(socket)
            {
            }

            ~cort_session() {}

            void on_read()  {}
            void on_close() {}
            void on_build() {}

        protected:
            std::shared_ptr<cort_socket> socket_;
    };
}
