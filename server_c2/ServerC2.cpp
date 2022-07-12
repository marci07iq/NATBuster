#include <iostream>
#include <memory>
#include <shared_mutex>


#include "../common/network/network.h"
#include "../common/utils/callbacks.h"

using namespace NATBuster::Common::Network;
using NATBuster::Common::Utils::MemberCallback;
using NATBuster::Common::Utils::Void;

class IPServer : public std::enable_shared_from_this<IPServer> {
    TCPSHandle _hwnd;
    std::shared_ptr<TCPSEmitter> _emitter;

    void connect_callback(Void data) {
        std::cout << "CONNECT AVAIL" << std::endl;
        NetworkAddress src;
        TCPCHandle client = _hwnd->accept(src);
        if (client) {
            std::cout << "ACCEPTED FROM " << src.get_ipv4() << ":" << src.get_port() << std::endl;
            client->close();
            std::cout << "CLOSED" << std::endl;
        }

    }

    void error_callback() {
        std::cout << "ERR" << std::endl;
    }

    void close_callback() {
        std::cout << "CLOSE" << std::endl;
    }

public:
    IPServer(uint16_t port) : _hwnd(std::make_shared<TCPS>("", port)), _emitter(std::make_shared<TCPSEmitter>(_hwnd)) {

    }

    void start() {
        _emitter->set_result_callback(new MemberCallback<IPServer, void, Void>(weak_from_this(), &IPServer::connect_callback));
        _emitter->set_error_callback(new MemberCallback<IPServer, void>(weak_from_this(), &IPServer::error_callback));
        _emitter->set_close_callback(new MemberCallback<IPServer, void>(weak_from_this(), &IPServer::close_callback));

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
    std::cin >> x;

    return 0;
}