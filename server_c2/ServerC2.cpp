#include <iostream>
#include <memory>
#include <shared_mutex>

#include "../common/network/network.h"
#include "../common/utils/callbacks.h"

#include "../common/utils/hex.h"

#include "c2_server.h"

using namespace NATBuster::Server;
using NATBuster::Common::Crypto::PKey;
using NATBuster::Common::Utils::Blob;
using NATBuster::Common::Identity::User;
using NATBuster::Common::Identity::UserGroup;
//usingg namespace NATBuster::Common::Network;
//using namespace NATBuster::Common::Transport;
//using NATBuster::Common::Utils::Void;
//using NATBuster::Common::Utils::MemberCallback;


std::shared_ptr<C2Server> server;

int main() {
    //Keys for testing the features
    std::string c2server_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIBX5mZoatkDBuIXD33zY0AWnBM3GHIO1egJrJdHAdDyZ\n-----END PRIVATE KEY-----";
    std::string client_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAdAjRiFt95Jev54VoKPaOiV48CokwJ9lKWOrMRpON1nY=\n-----END PUBLIC KEY-----";

    Blob c2server_private_key_b = Blob::factory_string(c2server_private_key_s);
    Blob client_public_key_b = Blob::factory_string(client_public_key_s);

    PKey c2server_private_key;
    PKey client_public_key;
    c2server_private_key.load_private(c2server_private_key_b);
    client_public_key.load_public(client_public_key_b);

    NATBuster::Common::Crypto::Hash fingerprint_hasher(NATBuster::Common::Crypto::HashAlgo::SHA256);
    Blob c2server_fingerprint;
    c2server_private_key.fingerprint(fingerprint_hasher, c2server_fingerprint);
    std::cout << "IP Server fingerprint: ";
    NATBuster::Common::Utils::print_hex(c2server_fingerprint);
    std::cout << std::endl;

    std::shared_ptr<User> client = std::make_shared<User>("client", std::move(client_public_key));

    //0: No authorised users objects: Implicity trust anyone
    std::shared_ptr<UserGroup> authorised_users0;
    //Empty object: Trust noone
    std::shared_ptr<UserGroup> authorised_users1 = std::make_shared<UserGroup>();
    //User given: Trust only that users
    std::shared_ptr<UserGroup> authorised_users2 = std::make_shared<UserGroup>();
    authorised_users2->addUser(client);

    server = std::make_shared<C2Server>(5987, authorised_users2, std::move(c2server_private_key));
    server->start();

    int x;
    do {
        std::cin >> x;
    } while (x != 0);

    server->_emitter->close();

    server->_emitter->join();

    return 0;
}