#include <iostream>

#include "../common/crypto/pkey.h"
#include "../common/utils/blob.h"

void keygen() {
    using namespace NATBuster::Common::Crypto;
    using NATBuster::Common::Utils::Blob;

    PKey key;
    key.generate_ec25519();

    std::cout << "Filename (no extension): " << std::endl;
    std::string filename;
    std::cin >> filename;

    key.save_file_public(filename + "_public.txt");
    key.save_file_private(filename + "_private.txt");
    
}

int main() {
    

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
            std::cout << "keygen: Create EC25519 keypair" << std::endl;
        }
        else if (command == "exit") {
            break;
        }
        else {
            std::cout << "Unknown command. Type help" << std::endl;
        }
    }
}