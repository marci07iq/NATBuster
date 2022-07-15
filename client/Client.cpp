#include <iostream>

#include "../common/crypto/pkey.h"
#include "../common/utils/blob.h"

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

int main() {
    //Keys for testing the features
    std::string client_private_key_s = "-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIGJOEK8OBASAmL7LKy0L5r4Md18JzK5jO9x5rNBXJHa1\n-----END PRIVATE KEY-----";
    std::string ipserver_public_key_s = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAvN41TKTecBATHqVhQzEmiT0ZDvXlEas9vFVR/aoztj0=\n-----END PUBLIC KEY-----";


    std::string command;
    while(true) {
        std::cout << "> ";
        std::cin >> command;

        if (command == "keygen") {
            keygen();
        } else if (command == "help" || command == "?") {
            std::cout << "Commands" << std::endl;
            std::cout << "exit: Exit" << std::endl;
            std::cout << "help, ?: Prints help" << std::endl;
            std::cout << "keygen: Create ED25519 keypair" << std::endl;
        }
        else if (command == "exit") {
            break;
        }
        else {
            std::cout << "Unknown command. Type help" << std::endl;
        }
    }
}