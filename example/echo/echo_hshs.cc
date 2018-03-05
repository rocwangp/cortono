#include "/home/roc/unix/cortono/cortono.h"

using namespace cortono::net;

class EchoSession : public SessionBase
{
    public:
        EchoSession(std::shared_ptr<Socket> socket)
            : SessionBase(socket)
        {

        }

        virtual void on_read(std::shared_ptr<Socket> socket) override
        {
            socket->send(socket->recv_all());
        }

};

int main()
{
    HSHA<EchoSession> hsha("localhost", 9999);
    hsha.start();
    return 0;
}
