#include <iostream>
#include <memory>
#include <shared_mutex>


#include "../common/network/network.h"
#include "../common/utils/callbacks.h"

using namespace NATBuster::Common::Network;
using NATBuster::Common::Utils::MemberWCallback;
using NATBuster::Common::Utils::Void;
using NATBuster::Common::Utils::Blob;

class IPServer : public std::enable_shared_from_this<IPServer> {
public:
    TCPSHandle _hwnd;
    std::shared_ptr<TCPSEmitter> _emitter;

    void connect_callback(Void data) {
        std::cout << "CONNECT AVAIL" << std::endl;
        TCPCHandle client = _hwnd->accept();
        if (client) {
            std::cout << "ACCEPTED FROM " << client->get_remote_addr().get_addr() << ":" << client->get_remote_addr().get_port() << std::endl;
            Blob resp = Blob::factory_string("ASD");
            client->send(resp);
            //client->close();
            //std::cout << "CLOSED" << std::endl;
        }

    }

    void error_callback() {
        std::cout << "ERR" << std::endl;
    }

    void close_callback() {
        std::cout << "CLOSE" << std::endl;
    }

    IPServer(uint16_t port) : _hwnd(std::make_shared<TCPS>("", port)), _emitter(std::make_shared<TCPSEmitter>(_hwnd)) {

    }

    void start() {
        _emitter->set_result_callback(new MemberWCallback<IPServer, void, Void>(weak_from_this(), &IPServer::connect_callback));
        _emitter->set_error_callback(new MemberWCallback<IPServer, void>(weak_from_this(), &IPServer::error_callback));
        _emitter->set_close_callback(new MemberWCallback<IPServer, void>(weak_from_this(), &IPServer::close_callback));

        _emitter->start();
    }

    ~IPServer() {
        std::cout << "ENDING" << std::endl;
    }
};

std::shared_ptr<IPServer> server;

int main() {
    server = std::make_shared<IPServer>(5987);
    server->start();

    int x;
    do {
        std::cin >> x;
    } while (x != 0);

    server->_emitter->close();

    server->_emitter->join();

    return 0;
}