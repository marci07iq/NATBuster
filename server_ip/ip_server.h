#pragma once

#include <set>

#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"

namespace NATBuster::Server {

    class IPServer;

    class IPServerEndpoint : public Common::Utils::SharedOnly<IPServerEndpoint> {
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
    public:
        //The currently open connections
        std::set<std::shared_ptr<IPServerEndpoint>> _connections;
        std::mutex _connection_lock;
        
        //Handle to the underlying socket
        Common::Network::TCPCHandleS _socket;
        //Handle to the underlying event emitter, to better control the thread
        std::shared_ptr<Common::Utils::EventEmitter> _emitter;
        //List of users authorised to use the server
        std::shared_ptr<Common::Identity::UserGroup> _authorised_users;
        //Identity of this server
        Common::Crypto::PKey _self;

        //Client emitter pool
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _client_emitter_provider;
        Common::Utils::shared_unique_ptr<Common::Utils::EventEmitter> _client_emitter;

        void accept_callback(Common::Network::TCPCHandleU&& socket);

        void error_callback(Common::ErrorCode code);

        void close_callback();

        IPServer(
            uint16_t port,
            std::shared_ptr<Common::Identity::UserGroup> authorised_users,
            Common::Crypto::PKey&& self
        );

        void start();

        ~IPServer();
    };
};