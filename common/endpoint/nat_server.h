#pragma once

#include "../network/network.h"

namespace NATBuster::Endpoint {
    class NATServerI;

    class NATServerO : public Utils::SharedOnly<NATServerO> {
        friend class Utils::SharedOnly<NATServerO>;
        friend class NATServerI;
        Network::UDPHandleS _socket;

        //Server event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _provider;
        std::shared_ptr<Utils::EventEmitter> _emitter;

        uint16_t _port;

        NATServerO(
            std::shared_ptr<Network::SocketEventEmitterProvider> provider,
            std::shared_ptr<Utils::EventEmitter> emitter,
            uint16_t port
        );

        void on_start();
        void on_error(ErrorCode);
        void on_close();

    public:
        void start();
    };

    class NATServerI : public Utils::SharedOnly<NATServerI> {
        friend class Utils::SharedOnly<NATServerI>;
        Network::UDPHandleS _socket;

        //Server event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _provider;
        std::shared_ptr<Utils::EventEmitter> _emitter;

        uint16_t _port;

        std::shared_ptr<NATServerO> _out_server;

        NATServerI(
            std::shared_ptr<Network::SocketEventEmitterProvider> provider,
            std::shared_ptr<Utils::EventEmitter> emitter,
            std::shared_ptr<NATServerO> out_server,
            uint16_t port
        );

        void on_start();
        void on_unfiltered_packet(const Utils::ConstBlobView& data, const Network::NetworkAddress& remote_address);
        void on_error(ErrorCode);
        void on_close();

    public:
        void start();
    };

    class NATServer : public Utils::SharedOnly<NATServer> {
        friend class Utils::SharedOnly<NATServer>;

        //Server event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _provider;
        std::shared_ptr<Utils::EventEmitter> _emitter;

        std::list<std::shared_ptr<NATServerO>> _sockets_o;
        std::list<std::shared_ptr<NATServerI>> _sockets_i;

        uint16_t _oport;
        std::vector<uint16_t> _iports;

        Utils::OnetimeWaker _waker;

        NATServer(
            uint16_t oport,
            std::vector<uint16_t> iports
        );

    public:

        void start();
        void stop();

        bool done();
        void wait();
    };
}