#include "/home/roc/unix/cortono/cortono.h"

using namespace cortono::net;

class EchoSession : public SessionBase
{
    public:
        EchoSession(std::shared_ptr<Socket> socket)
            : SessionBase(socket)
        {

        }

        void on_recv(std::shared_ptr<Socket> socket)
        {
            socket->send(socket->recv_all());
        }
};

int main()
{
    EventLoop base;
    Service<EchoSession> server(&base, "localhost", 9999);
    server.start();
    base.sync_loop();
    return 0;
}
