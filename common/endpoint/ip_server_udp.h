#pragma once

#include "../network/network.h"

namespace NATBuster::Endpoint {
    class IPServerUDP : public Utils::SharedOnly<IPServerUDP>{
        friend class Utils::SharedOnly<IPServerUDP>;
        Network::UDPHandleS _socket;

        //Server event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _server_emitter_provider;
        std::shared_ptr<Utils::EventEmitter> _server_emitter;

        Utils::OnetimeWaker _waker;

        uint16_t _port;

        IPServerUDP(
            uint16_t port
        );

        void on_start();
        void on_unfiltered_packet(const Utils::ConstBlobView& data, const Network::NetworkAddress& remote_address);
        void on_error(ErrorCode);
        void on_close();

    public:
       
        void start();
        void stop();

        bool done();
        void wait();
    };
}