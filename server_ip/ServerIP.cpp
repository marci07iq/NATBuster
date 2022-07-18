#include <iostream>
#include <memory>
#include <shared_mutex>

#include "../common/network/network.h"
#include "../common/utils/callbacks.h"

#include "../common/utils/hex.h"

#include "ip_server.h"

using namespace NATBuster::Server;
using NATBuster::Common::Crypto::PKey;
using NATBuster::Common::Utils::Blob;
using NATBuster::Common::Identity::User;
using NATBuster::Common::Identity::UserGroup;
//usingg namespace NATBuster::Common::Network;
//using namespace NATBuster::Common::Transport;
//using NATBuster::Common::Utils::Void;
//using NATBuster::Common::Utils::MemberCallback;


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

    NATBuster::Common::Crypto::Hash fingerprint_hasher(NATBuster::Common::Crypto::HashAlgo::SHA256);
    Blob ipserver_fingerprint;
    ipserver_private_key.fingerprint(fingerprint_hasher, ipserver_fingerprint);
    std::cout << "IP Server fingerprint: ";
    NATBuster::Common::Utils::print_hex(ipserver_fingerprint);
    std::cout << std::endl;

    std::shared_ptr<User> client = std::make_shared<User>("client", std::move(client_public_key));

    //0: No authorised users objects: Implicity trust anyone
    std::shared_ptr<UserGroup> authorised_users0;
    //Empty object: Trust noone
    std::shared_ptr<UserGroup> authorised_users1 = std::make_shared<UserGroup>();
    //User given: Trust only that users
    std::shared_ptr<UserGroup> authorised_users2 = std::make_shared<UserGroup>();
    authorised_users2->addUser(client);
    
    server = std::make_shared<IPServer>(5987, authorised_users2, std::move(ipserver_private_key));
    server->start();

    int x;
    do {
        std::cin >> x;
    } while (x != 0);

    server->_emitter->close();

    server->_emitter->join();

    return 0;
}