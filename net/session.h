#pragma once

#include <memory>
#include "socket.h"

namespace cortono
{
    namespace net
    {
        class Socket;

        class SessionBase : public std::enable_shared_from_this<SessionBase>
        {
            public:
                SessionBase(std::shared_ptr<Socket> socket)
                    : socket_(socket)
                {

                }

                virtual ~SessionBase() {}

                virtual void on_recv(std::shared_ptr<Socket>) {}
                virtual void on_close() {}
                virtual void on_build() {}

            public:
                std::string name() const { return socket_->local_address() + socket_->peer_address(); }

            protected:
                std::shared_ptr<Socket> socket_;
        };
    }
}
