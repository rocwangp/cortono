#include "/home/roc/unix/cortono/cortono.hpp"
using namespace cortono::net;

class echo_session : public tcp_session
{
    public:
        echo_session(std::shared_ptr<tcp_socket> socket)
            : tcp_session(socket)
        {
            set_read_callback(std::bind(&echo_session::handle_read, this));
        }

        void handle_read() {
            socket_->write(socket_->read_all());
        }
};

int main()
{
    event_loop base;
    tcp_service<echo_session> server{ &base, "localhost", 9999 };
    server.on_conn([&server](auto socket) {
        server.register_session(socket , std::make_shared<echo_session>(socket)); });
    server.start();
    base.sync_loop();
    return 0;
}
