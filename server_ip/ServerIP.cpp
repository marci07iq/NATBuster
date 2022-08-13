#include <iostream>
#include <memory>
#include <shared_mutex>

#include "../common/network/network.h"
#include "../common/utils/callbacks.h"

#include "../common/utils/hex.h"

#include "../common/endpoint/ip_server.h"
#include "../common/endpoint/ip_server_udp.h"

using NATBuster::Crypto::PKey;
using NATBuster::Crypto::PuKey;
using NATBuster::Crypto::PrKey;
using NATBuster::Utils::Blob;
using NATBuster::Identity::User;
using NATBuster::Identity::UserGroup;
using NATBuster::Endpoint::IPServer;
using NATBuster::Endpoint::IPServerUDP;
//usingg namespace NATBuster::Network;
//using namespace NATBuster::Transport;
//using NATBuster::Utils::Void;
//using NATBuster::Utils::MemberCallback;


//std::shared_ptr<IPServer> server;

int main() {
    //Keys for testing the features
    std::string ipserver_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIL3OPF/F3rN7Pwe/TzKMtyZeKC1WxDyy0kwgK/BxbfP4\n-----END PRIVATE KEY-----";
    std::string client1_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAdAjRiFt95Jev54VoKPaOiV48CokwJ9lKWOrMRpON1nY=\n-----END PUBLIC KEY-----";
    std::string client2_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAQHQNt6AhqhFJYYMi4M0Li1Ny8KMI3+scg+PgzbuhF3M=\n-----END PUBLIC KEY-----";

    Blob ipserver_private_key_b = Blob::factory_string(ipserver_private_key_s);
    Blob client1_public_key_b = Blob::factory_string(client1_public_key_s);
    Blob client2_public_key_b = Blob::factory_string(client2_public_key_s);

    std::shared_ptr<PrKey> ipserver_private_key = std::make_shared<PrKey>();
    ipserver_private_key->load_private(ipserver_private_key_b);
    std::shared_ptr<PuKey> client1_public_key = std::make_shared<PuKey>();
    client1_public_key->load_public(client1_public_key_b);
    std::shared_ptr<PuKey> client2_public_key = std::make_shared<PuKey>();
    client2_public_key->load_public(client2_public_key_b);

    Blob ipserver_fingerprint;
    ipserver_private_key->fingerprint(ipserver_fingerprint);
    std::cout << "C2 Server fingerprint: ";
    NATBuster::Utils::print_hex(ipserver_fingerprint);
    std::cout << std::endl;

    std::shared_ptr<User> client1 = std::make_shared<User>("client1", client1_public_key);
    std::shared_ptr<User> client2 = std::make_shared<User>("client2", client2_public_key);

    //0: No authorised users objects: Implicity trust anyone
    std::shared_ptr<UserGroup> authorised_users0;
    //Empty object: Trust noone
    std::shared_ptr<UserGroup> authorised_users1 = std::make_shared<UserGroup>();
    //User given: Trust only that users
    std::shared_ptr<UserGroup> authorised_users2 = std::make_shared<UserGroup>();
    authorised_users2->addUser(client1);
    authorised_users2->addUser(client2);

    //server = std::make_shared<IPServer>((uint16_t)5987, authorised_users2, std::move(ipserver_private_key));

    /*int x;
    do {
        std::cin >> x;
    } while (x != 0);

    server->_server_emitter->stop();

    server->_server_emitter->join();*/


    std::shared_ptr<IPServerUDP> server1 = IPServerUDP::create((uint16_t)5987);
    std::shared_ptr<IPServerUDP> server2 = IPServerUDP::create((uint16_t)5988);

    server1->start();
    server2->start();

    int x;
    do {
        std::cout << "Type 42 to exit: " << std::endl;
        std::cin >> x;
    } while (x != 42);


    server1->stop();
    server2->stop();

    server1->wait();
    server2->wait();

    return 0;
}