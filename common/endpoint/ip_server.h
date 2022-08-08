#pragma once

#include <set>

#include "../transport/opt_tcp.h"
#include "../transport/opt_session.h"

namespace NATBuster::Endpoint {

    class IPServer;

    class IPServerEndpoint : public Utils::SharedOnly<IPServerEndpoint> {
        friend Utils::SharedOnly<IPServerEndpoint>;
        //The socket, to get address from
        Network::TCPCHandleS _socket;
        //Underliyng OPT
        std::shared_ptr<Transport::OPTBase> _underlying;
        //The main server pool
        std::shared_ptr<IPServer> _server;

        //Called when a packet can be read
        void on_open();
        //Timer
        void on_timeout();
        //Socket was closed
        void on_close();

        IPServerEndpoint(
            Network::TCPCHandleS socket,
            std::shared_ptr<Transport::OPTBase> underlying,
            std::shared_ptr<IPServer> server);

        void start();

        void init() override;
    public:
    };

    class IPServer : public Utils::SharedOnly<IPServer> {
        friend Utils::SharedOnly<IPServer>;
    public:
        //The currently open connections
        std::set<std::shared_ptr<IPServerEndpoint>> _connections;
        std::mutex _connection_lock;
        
        uint16_t _port;
        //Handle to the underlying socket
        Network::TCPSHandleS _socket;
        //Server event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _server_emitter_provider;
        std::shared_ptr<Utils::EventEmitter> _server_emitter;
        //Client event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _client_emitter_provider;
        Utils::shared_unique_ptr<Utils::EventEmitter> _client_emitter;

        //List of users authorised to use the server
        const std::shared_ptr<const Identity::UserGroup> _authorised_users;
        //Identity of this server
        const std::shared_ptr<const Crypto::PrKey> _self;


        void on_accept(Network::TCPCHandleU&& socket);

        void on_error(ErrorCode code);

        void on_close();

        IPServer(
            uint16_t port,
            const std::shared_ptr<const Identity::UserGroup> authorised_users,
            const std::shared_ptr<const Crypto::PrKey> self
        );

        void start();

        ~IPServer();
    };
};