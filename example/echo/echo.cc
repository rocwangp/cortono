#include "/home/roc/unix/cortono/cortono.h"
using namespace cortono::net;

class echo_session : public cort_session
{
    public:
        echo_session(std::shared_ptr<cort_socket> socket)
            : cort_session(socket)
        {

        }

        void on_read() {
            socket_->write(socket_->read_all());
        }
};

int main()
{
    cort_eventloop base;
    cort_service<echo_session> server{ &base, "localhost", 9999 };
    server.on_conn([&server](auto socket) {
        server.register_session(socket , std::make_shared<echo_session>(socket));
    });
    server.start();
    base.sync_loop();
    return 0;
}
