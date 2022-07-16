#include <iostream>

#include "../common/crypto/pkey.h"
#include "../common/utils/blob.h"

#include "ip_client.h"

using namespace NATBuster::Client;
using NATBuster::Common::Crypto::PKey;
using NATBuster::Common::Utils::Blob;
using NATBuster::Common::Identity::User;
using NATBuster::Common::Identity::UserGroup;


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

int main() {
    //Keys for testing the features
    std::string client_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIGJOEK8OBASAmL7LKy0L5r4Md18JzK5jO9x5rNBXJHa1\n-----END PRIVATE KEY-----";
    std::string ipserver_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAvN41TKTecBATHqVhQzEmiT0ZDvXlEas9vFVR/aoztj0=\n-----END PUBLIC KEY-----";

    Blob client_private_key_b = Blob::factory_string(client_private_key_s);
    Blob ipserver_public_key_b = Blob::factory_string(ipserver_public_key_s);

    PKey client_private_key;
    PKey ipserver_public_key;
    client_private_key.load_private(client_private_key_b);
    ipserver_public_key.load_public(ipserver_public_key_b);

    std::shared_ptr<User> ipserver = std::make_shared<User>("ipserver", std::move(ipserver_public_key));

    std::shared_ptr<UserGroup> authorised_ip_servers = std::make_shared<UserGroup>();
    authorised_ip_servers->addUser(ipserver);

    std::string command;
    while(true) {
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
        else {
            std::cout << "Unknown command. Type help" << std::endl;
        }
    }
}