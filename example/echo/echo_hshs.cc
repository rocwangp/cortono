#include "/home/roc/unix/cortono/cortono.hpp"
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
    cort_hsha<echo_session> hsha("localhost", 9999);
    hsha.on_conn([&hsha](auto socket) {
        hsha.register_session(socket, std::make_shared<echo_session>(socket));
    });
    hsha.start();
    return 0;
}
