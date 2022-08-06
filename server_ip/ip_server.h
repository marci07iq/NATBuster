#pragma once

#include <set>

#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"

namespace NATBuster::Server {

    class IPServer;

    class IPServerEndpoint : public Common::Utils::SharedOnly<IPServerEndpoint> {
        friend Common::Utils::SharedOnly<IPServerEndpoint>;
        //The socket, to get address from
        Common::Network::TCPCHandleS _socket;
        //Underliyng OPT
        std::shared_ptr<Common::Transport::OPTBase> _underlying;
        //The main server pool
        std::shared_ptr<IPServer> _server;

        //Called when a packet can be read
        void on_open();
        //Timer
        void on_timeout();
        //Socket was closed
        void on_close();

        IPServerEndpoint(
            Common::Network::TCPCHandleS socket,
            std::shared_ptr<Common::Transport::OPTBase> underlying,
            std::shared_ptr<IPServer> server);

        void start();

        void init() override;
    public:
    };

    class IPServer : public Common::Utils::SharedOnly<IPServer> {
        friend Common::Utils::SharedOnly<IPServer>;
    public:
        //The currently open connections
        std::set<std::shared_ptr<IPServerEndpoint>> _connections;
        std::mutex _connection_lock;
        
        uint16_t _port;
        //Handle to the underlying socket
        Common::Network::TCPSHandleS _socket;
        //Server event emitter
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _server_emitter_provider;
        std::shared_ptr<Common::Utils::EventEmitter> _server_emitter;
        //Client event emitter
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _client_emitter_provider;
        Common::Utils::shared_unique_ptr<Common::Utils::EventEmitter> _client_emitter;

        //List of users authorised to use the server
        const std::shared_ptr<const Common::Identity::UserGroup> _authorised_users;
        //Identity of this server
        const std::shared_ptr<const Common::Crypto::PrKey> _self;


        void on_accept(Common::Network::TCPCHandleU&& socket);

        void on_error(Common::ErrorCode code);

        void on_close();

        IPServer(
            uint16_t port,
            const std::shared_ptr<const Common::Identity::UserGroup> authorised_users,
            const std::shared_ptr<const Common::Crypto::PrKey> self
        );

        void start();

        ~IPServer();
    };
};