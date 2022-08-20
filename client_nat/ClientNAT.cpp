#include <iostream>

#include "../common/config/config.h"

#include "../common/crypto/pkey.h"
#include "../common/utils/blob.h"

#include "../common/utils/hex.h"

#include "../common/endpoint/nat_client.h"

using NATBuster::Crypto::PKey;
using NATBuster::Crypto::PrKey;
using NATBuster::Crypto::PuKey;
using NATBuster::Utils::Blob;
using NATBuster::Network::SocketEventEmitterProvider;
using NATBuster::Utils::EventEmitter;
using NATBuster::Endpoint::NATClient;
using NATBuster::Utils::shared_unique_ptr;


int main() {
    std::cout << "Starting threads" << std::endl;

    shared_unique_ptr<SocketEventEmitterProvider> provider_u =
        SocketEventEmitterProvider::create();
    std::shared_ptr< SocketEventEmitterProvider> provider = provider_u.get_shared();

    shared_unique_ptr<EventEmitter> emitter = EventEmitter::create();
    emitter->start_async(std::move(provider_u));


    std::list<NATClient::host_data> hosts;
    //hosts.push_back(NATClient::host_data{ .name = "172.25.252.62", .port = 5987 });
    //hosts.push_back(NATClient::host_data{ .name = "172.25.252.62", .port = 5988 });
    hosts.push_back(NATClient::host_data{ .name = "172.25.240.1", .port = 5987 });
    hosts.push_back(NATClient::host_data{ .name = "172.25.240.1", .port = 5988 });
    //for (int i = 1; i < argc; i++) {
    /*while (true) {
        std::string s;
        std::cout << "Enter address: ";
        std::cin >> s;
        if (s.size() > 1) {
            hosts.push_back(NATClient::host_data{ .name = s, .port = 5987 });
            hosts.push_back(NATClient::host_data{ .name = s, .port = 5987 });
        }
        else {
            break;
        }
    }*/

    std::shared_ptr<NATClient> query = NATClient::create(
        hosts,
        provider, emitter.get_shared());

    query->wait();

    auto res = query->get_results();

    for (auto& it : res) {
        if (it->_foreign) {
            std::cout << "Unexpected reply" << std::endl;
        }
        else {
            std::cout << "Fetching from " << it->_remote_host.name << ":" << it->_remote_host.port << std::endl;
        }
        std::cout << it->_code << std::endl;
        if (it->_code == NATBuster::ErrorCode::OK) {
            if (it->_foreign) {
                
                std::cout << "External: " << it->_my_address << std::endl;
                std::cout << "Remote: " << it->_remote_address << std::endl;
            }
            else {
                std::cout << "Local: " << it->_local_address << std::endl;
                std::cout << "External: " << it->_my_address << std::endl;
                std::cout << "Remote: " << it->_remote_address << std::endl;
            }
        }
        std::cout << std::endl;
    }
}