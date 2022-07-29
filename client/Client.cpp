#include <iostream>

#include "../common/crypto/pkey.h"
#include "../common/utils/blob.h"

#include "../common/utils/hex.h"

#include "ip_client.h"
#include "c2_client.h"
#include "router.h"
#include "punch_sym_sym.h"

using namespace NATBuster::Client;
using NATBuster::Common::Crypto::PKey;
using NATBuster::Common::Utils::Blob;
using NATBuster::Common::Identity::User;
using NATBuster::Common::Identity::UserGroup;
using NATBuster::Client::HolepunchSym;

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

    std::shared_ptr<NATBuster::Client::IPClient> query = NATBuster::Client::IPClient::create("127.0.0.1", 5987, auth_servers, std::move(self));

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

    c2_instance = NATBuster::Client::C2Client::create("127.0.0.1", 5987, c2_servers, c2_clients, std::move(self));
}

std::shared_ptr<NATBuster::Client::C2Client> c2_pipe_instance;

void open_pipe() {

}

void punch() {
    std::string remote;
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
    }
}

int main() {
    //Keys for testing the features
    std::string client_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIGJOEK8OBASAmL7LKy0L5r4Md18JzK5jO9x5rNBXJHa1\n-----END PRIVATE KEY-----";
    std::string ipserver_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAvN41TKTecBATHqVhQzEmiT0ZDvXlEas9vFVR/aoztj0=\n-----END PUBLIC KEY-----";
    std::string c2server_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAI8tKwhR134ftj9Wa9S97Iu8SEez+ThY34ifnid1+4OU=\n-----END PUBLIC KEY-----";

    Blob client_private_key_b = Blob::factory_string(client_private_key_s);
    Blob ipserver_public_key_b = Blob::factory_string(ipserver_public_key_s);
    Blob c2server_public_key_b = Blob::factory_string(c2server_public_key_s);

    PKey client_private_key;
    PKey ipserver_public_key;
    PKey c2server_public_key;
    client_private_key.load_private(client_private_key_b);
    ipserver_public_key.load_public(ipserver_public_key_b);
    c2server_public_key.load_public(c2server_public_key_b);

    Blob client_fingerprint;
    client_private_key.fingerprint(client_fingerprint);
    std::cout << "Client fingerprint: ";
    NATBuster::Common::Utils::print_hex(client_fingerprint);
    std::cout << std::endl;

    std::shared_ptr<User> client = std::make_shared<User>("client1", std::move(client_private_key));
    std::shared_ptr<User> ipserver = std::make_shared<User>("ipserver", std::move(ipserver_public_key));
    std::shared_ptr<User> c2server = std::make_shared<User>("c2server", std::move(c2server_public_key));

    std::shared_ptr<UserGroup> authorised_c2_clients = std::make_shared<UserGroup>();
    authorised_c2_clients->addUser(client);

    std::shared_ptr<UserGroup> authorised_ip_servers = std::make_shared<UserGroup>();
    authorised_ip_servers->addUser(ipserver);

    std::shared_ptr<UserGroup> authorised_c2_servers = std::make_shared<UserGroup>();
    authorised_c2_servers->addUser(c2server);

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
        else if (command == "open_pipe") {
            //c2_instance->
        }
        else if (command == "punch") {
            punch();
        }
        else {
            std::cout << "Unknown command. Type help" << std::endl;
        }
    }
}