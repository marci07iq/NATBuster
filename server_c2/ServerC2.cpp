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
    std::string client1_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAdAjRiFt95Jev54VoKPaOiV48CokwJ9lKWOrMRpON1nY=\n-----END PUBLIC KEY-----";
    std::string client2_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAQHQNt6AhqhFJYYMi4M0Li1Ny8KMI3+scg+PgzbuhF3M=\n-----END PUBLIC KEY-----";

    Blob c2server_private_key_b = Blob::factory_string(c2server_private_key_s);
    Blob client1_public_key_b = Blob::factory_string(client1_public_key_s);
    Blob client2_public_key_b = Blob::factory_string(client2_public_key_s);

    PKey c2server_private_key;
    c2server_private_key.load_private(c2server_private_key_b);
    PKey client1_public_key;
    client1_public_key.load_public(client1_public_key_b);
    PKey client2_public_key;
    client2_public_key.load_public(client2_public_key_b);

    Blob c2server_fingerprint;
    c2server_private_key.fingerprint(c2server_fingerprint);
    std::cout << "IP Server fingerprint: ";
    NATBuster::Common::Utils::print_hex(c2server_fingerprint);
    std::cout << std::endl;

    std::shared_ptr<User> client1 = std::make_shared<User>("client1", std::move(client1_public_key));
    std::shared_ptr<User> client2 = std::make_shared<User>("client2", std::move(client2_public_key));

    //0: No authorised users objects: Implicity trust anyone
    std::shared_ptr<UserGroup> authorised_users0;
    //Empty object: Trust noone
    std::shared_ptr<UserGroup> authorised_users1 = std::make_shared<UserGroup>();
    //User given: Trust only that users
    std::shared_ptr<UserGroup> authorised_users2 = std::make_shared<UserGroup>();
    authorised_users2->addUser(client1);
    authorised_users2->addUser(client2);

    server = std::make_shared<C2Server>(5987, authorised_users2, std::move(c2server_private_key));
    server->start();

    int x;
    do {
        std::cin >> x;
    } while (x != 0);

    server->_server_emitter->stop();
    server->_client_emitter->stop();

    server->_server_emitter->join();
    server->_client_emitter->join();

    return 0;
}