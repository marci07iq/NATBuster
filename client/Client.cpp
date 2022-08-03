#include <iostream>

#include "../common/crypto/pkey.h"
#include "../common/utils/blob.h"

#include "../common/utils/hex.h"

#include "ip_client.h"
#include "c2_client.h"
#include "router.h"
#include "punch_sym_sym.h"

using namespace NATBuster::Client;
using namespace NATBuster::Common;
using NATBuster::Common::Crypto::PKey;
using NATBuster::Common::Utils::Blob;
using NATBuster::Common::Identity::User;
using NATBuster::Common::Identity::UserGroup;
using NATBuster::Client::HolepunchSym;

std::shared_ptr<Network::SocketEventEmitterProvider> main_provider;
std::shared_ptr<Utils::EventEmitter> main_emitter;

void keygen() {
    using namespace NATBuster::Common::Crypto;
    using NATBuster::Common::Utils::Blob;

    PKey key;
    key.generate_ed25519();

    std::cout << "Filename (no extension): " << std::endl;
    std::string filename;
    std::cin >> filename;

    key.export_file_public(filename + "_public.txt");
    key.export_file_private(filename + "_private.txt");

}

void get_ip(std::shared_ptr<UserGroup> auth_servers, PKey&& self) {
    std::cout << "Querying from 127.0.0.1:5987" << std::endl;

    std::shared_ptr<NATBuster::Client::IPClient> query = NATBuster::Client::IPClient::create(
        "127.0.0.1", 5987,
        main_provider, main_emitter,
        auth_servers, std::move(self));

    query->wait();

    if (query->get_success()) {
        std::cout << "Success, self identity is: " << query->get_my_ip().value() << ":" << query->get_my_port().value() << std::endl;
    }
    else {
        std::cout << "ERROR" << std::endl;
    }
}

std::shared_ptr<NATBuster::Client::C2Client> c2_instance;

void login(std::shared_ptr<UserGroup> c2_servers, std::shared_ptr<UserGroup> c2_clients, PKey&& self) {
    std::cout << "Querying from 127.0.0.1:5987" << std::endl;

    c2_instance = NATBuster::Client::C2Client::create(
        "127.0.0.1", 5987,
        main_provider, main_emitter,
        c2_servers, c2_clients, std::move(self));
}

//std::shared_ptr<NATBuster::Client::Router> router;

void punch(PKey&& self, std::shared_ptr<User> remote_user) {
    /*std::string remote;
    std::cout << "Remote address: ";
    std::cin >> remote;

    std::string magic_ob;
    std::cout << "Outbound magic: ";
    std::cin >> magic_ob;
    std::string magic_ib;
    std::cout << "Inbound magic: ";
    std::cin >> magic_ib;

    bool asd;
    std::cout << "GO?";
    std::cin >> asd;

    if (asd) {
        std::shared_ptr<HolepunchSym> punch = HolepunchSym::create(remote, Blob::factory_string(magic_ob), Blob::factory_string(magic_ib));
        punch->sync_launch();
        if (punch->done()) {
            std::cout << "Done" << std::endl;
            NATBuster::Common::Network::UDPHandle socket = punch->get_socket();
            if (socket && socket->valid()) {
                std::cout << "Success" << std::endl;
                socket->send(Blob::factory_string("Hello!"));
            }
        }
        else {
            std::cout << "Error" << std::endl;
        }
    }*/
    c2_instance->punch(std::move(self), remote_user);
}

void forward() {
    std::cout << "Source port: ";
    uint16_t local_port;
    std::cin >> local_port;
    std::cout << "Destination port: ";
    uint16_t remote_port;
    std::cin >> remote_port;

    std::lock_guard _lg(c2_instance->_open_routers_lock);

    NATBuster::Client::RouterTCPS::create(
        c2_instance->_open_routers.front(), local_port, remote_port);
}


int main() {
    //Keys for testing the features
    std::string client1_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIGJOEK8OBASAmL7LKy0L5r4Md18JzK5jO9x5rNBXJHa1\n-----END PRIVATE KEY-----";
    std::string client2_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIC96lygW3jpzwL7tpDydgRtIySnPX8ISLiGZD/0y3R38\n-----END PRIVATE KEY-----";
    std::string client1_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAdAjRiFt95Jev54VoKPaOiV48CokwJ9lKWOrMRpON1nY=\n-----END PUBLIC KEY-----";
    std::string client2_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAQHQNt6AhqhFJYYMi4M0Li1Ny8KMI3+scg+PgzbuhF3M=\n-----END PUBLIC KEY-----";
    std::string ipserver_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAvN41TKTecBATHqVhQzEmiT0ZDvXlEas9vFVR/aoztj0=\n-----END PUBLIC KEY-----";
    std::string c2server_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAI8tKwhR134ftj9Wa9S97Iu8SEez+ThY34ifnid1+4OU=\n-----END PUBLIC KEY-----";

    std::cout << "Client #?";
    int client_num;
    std::cin >> client_num;
    std::string client_private_key_s = (client_num == 1) ? client1_private_key_s : client2_private_key_s;
    std::string remote_public_key_s = (client_num == 1) ? client2_public_key_s : client1_public_key_s;

    Blob client_private_key_b = Blob::factory_string(client_private_key_s);
    Blob remote_public_key_b = Blob::factory_string(remote_public_key_s);
    Blob ipserver_public_key_b = Blob::factory_string(ipserver_public_key_s);
    Blob c2server_public_key_b = Blob::factory_string(c2server_public_key_s);

    PKey client_private_key;
    PKey remote_public_key;
    PKey ipserver_public_key;
    PKey c2server_public_key;
    client_private_key.load_private(client_private_key_b);
    remote_public_key.load_public(remote_public_key_b);
    ipserver_public_key.load_public(ipserver_public_key_b);
    c2server_public_key.load_public(c2server_public_key_b);

    Blob client_fingerprint;
    client_private_key.fingerprint(client_fingerprint);
    std::cout << "Client fingerprint: ";
    NATBuster::Common::Utils::print_hex(client_fingerprint);
    std::cout << std::endl;

    PKey self_copy;
    self_copy.copy_public_from(client_private_key);
    std::shared_ptr<User> client = std::make_shared<User>("client", std::move(self_copy));
    std::shared_ptr<User> remote = std::make_shared<User>("remote", std::move(remote_public_key));
    std::shared_ptr<User> ipserver = std::make_shared<User>("ipserver", std::move(ipserver_public_key));
    std::shared_ptr<User> c2server = std::make_shared<User>("c2server", std::move(c2server_public_key));

    std::shared_ptr<UserGroup> authorised_c2_clients = std::make_shared<UserGroup>();
    authorised_c2_clients->addUser(remote);

    std::shared_ptr<UserGroup> authorised_ip_servers = std::make_shared<UserGroup>();
    authorised_ip_servers->addUser(ipserver);

    std::shared_ptr<UserGroup> authorised_c2_servers = std::make_shared<UserGroup>();
    authorised_c2_servers->addUser(c2server);

    std::cout << "Starting threads" << std::endl;
    {
        Utils::shared_unique_ptr<Network::SocketEventEmitterProvider> provider =
            Network::SocketEventEmitterProvider::create();
        main_provider = provider;

        main_emitter = Utils::EventEmitter::create();
        main_emitter->start_async(std::move(provider));
    }

    std::string command;
    while (true) {
        std::cout << "> ";
        std::cin >> command;

        if (command == "exit") {
            break;
        }
        if (command == "get_ip") {
            PKey self_copy;
            self_copy.copy_private_from(client_private_key);
            get_ip(authorised_ip_servers, std::move(self_copy));
        }
        else if (command == "help" || command == "?") {
            std::cout << "Commands" << std::endl;
            std::cout << "exit: Exit" << std::endl;
            std::cout << "get_ip: Get IP from IPServer" << std::endl;
            std::cout << "help, ?: Prints help" << std::endl;
            std::cout << "keygen: Create ED25519 keypair" << std::endl;
        }
        else if (command == "keygen") {
            keygen();
        }
        else if (command == "login") {
            PKey self_copy;
            self_copy.copy_private_from(client_private_key);
            login(authorised_c2_servers, authorised_c2_clients, std::move(self_copy));
        }
        else if (command == "forward") {
            forward();
        }
        else if (command == "punch") {
            PKey self_copy;
            self_copy.copy_private_from(client_private_key);
            punch(std::move(self_copy), remote);
        }
        else {
            std::cout << "Unknown command. Type help" << std::endl;
        }
    }
}