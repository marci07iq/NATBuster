#include <iostream>
#include <memory>
#include <shared_mutex>


#include "../common/network/network.h"
#include "../common/utils/callbacks.h"
#include "../common/transport/opt_tcp.h"
#include "../common/transport/session.h"

using namespace NATBuster::Common::Network;
using namespace NATBuster::Common::Transport;
using NATBuster::Common::Crypto::PKey;
using NATBuster::Common::Utils::MemberCallback;
using NATBuster::Common::Utils::Void;
using NATBuster::Common::Utils::Blob;
using NATBuster::Common::Identity::UserGroup;
using NATBuster::Common::Identity::User;

class IPServer : public std::enable_shared_from_this<IPServer> {


public:
    TCPSHandle _hwnd;
    std::shared_ptr<TCPSEmitter> _emitter;
    std::shared_ptr<UserGroup> _authorised_users;
    PKey _self;

    void connect_callback(Void data) {
        std::cout << "CONNECT AVAIL" << std::endl;
        TCPCHandle client = _hwnd->accept();
        if (client) {
            std::cout << "ACCEPTED FROM " << client->get_remote_addr().get_addr() << ":" << client->get_remote_addr().get_port() << std::endl;
            //Create OPT TCP
            OPTTCPHandle opt = OPTTCP::create(false, client);
            //Create OPT Session
            std::shared_ptr<Session> session = std::make_shared<Session>(false, opt, _self.copy_private(), _authorised_users);
            session->start();
            
        }

    }

    void error_callback() {
        std::cout << "ERR" << std::endl;
    }

    void close_callback() {
        std::cout << "CLOSE" << std::endl;
    }

    IPServer(
        uint16_t port,
        std::shared_ptr<UserGroup> authorised_users,
        PKey&& self
        ) : 
        _hwnd(std::make_shared<TCPS>("", port)),
        _emitter(std::make_shared<TCPSEmitter>(_hwnd)),
        _authorised_users(authorised_users),
        _self(std::move(self)) {

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
    //Keys for testing the features
    std::string ipserver_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIL3OPF/F3rN7Pwe/TzKMtyZeKC1WxDyy0kwgK/BxbfP4\n-----END PRIVATE KEY-----";
    std::string client_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAdAjRiFt95Jev54VoKPaOiV48CokwJ9lKWOrMRpON1nY=\n-----END PUBLIC KEY-----";

    Blob ipserver_private_key_b = Blob::factory_string(ipserver_private_key_s);
    Blob client_public_key_b = Blob::factory_string(client_public_key_s);

    PKey ipserver_private_key;
    PKey client_public_key;
    ipserver_private_key.load_private(ipserver_private_key_b);
    client_public_key.load_public(client_public_key_b);

    std::shared_ptr<UserGroup> authorised_users;
    
    server = std::make_shared<IPServer>(5987, authorised_users, std::move(ipserver_private_key));
    server->start();

    int x;
    do {
        std::cin >> x;
    } while (x != 0);

    server->_emitter->close();

    server->_emitter->join();

    return 0;
}