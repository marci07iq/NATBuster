#include <iostream>

#include "../common/config/config.h"

#include "../common/crypto/pkey.h"
#include "../common/utils/blob.h"

#include "../common/utils/hex.h"

#include "../common/endpoint/ip_client_udp.h"

using NATBuster::Crypto::PKey;
using NATBuster::Crypto::PrKey;
using NATBuster::Crypto::PuKey;
using NATBuster::Utils::Blob;
using NATBuster::Network::SocketEventEmitterProvider;
using NATBuster::Utils::EventEmitter;
using NATBuster::Endpoint::IPClientUDP;
using NATBuster::Utils::shared_unique_ptr;


int main() {
    std::cout << "Starting threads" << std::endl;

    shared_unique_ptr<SocketEventEmitterProvider> provider_u =
        SocketEventEmitterProvider::create();
    std::shared_ptr< SocketEventEmitterProvider> provider = provider_u.get_shared();

    shared_unique_ptr<EventEmitter> emitter = EventEmitter::create();
    emitter->start_async(std::move(provider_u));


    std::list<IPClientUDP::host_data> hosts = {
        IPClientUDP::host_data{.name = "127.0.0.1", .port = 5987},
        IPClientUDP::host_data{.name = "127.0.0.1", .port = 5988},
    };

    std::shared_ptr<IPClientUDP> query = IPClientUDP::create(
        hosts,
        provider, emitter.get_shared());

    query->wait();

    auto res = query->get_results();

    for (auto& it : res) {
        std::cout << "Fetching from " << it->_remote_host.name << ":" << it->_remote_host.port << std::endl;
        std::cout << it->_code << std::endl;
        if (it->_code == NATBuster::ErrorCode::OK) {
            std::cout << "Local: " << it->_local_address << std::endl;
            std::cout << "External: " << it->_my_address << std::endl;
            std::cout << "Remote: " << it->_remote_address << std::endl;
        }
        std::cout << std::endl;
    }
}