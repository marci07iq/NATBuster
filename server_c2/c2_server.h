#pragma once

#include <set>

#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"
#include "../common/transport/opt_pipes.h"

#include "../common/transport/opt_c2.h"

namespace NATBuster::Server {

    class C2Server;
    class C2ServerEndpoint;
    class C2ServerRoute;



    class C2Server : public Common::Utils::SharedOnly<C2Server> {
        friend class Common::Utils::SharedOnly<C2Server>;
    public:
        std::list<std::shared_ptr<C2ServerRoute>> _routes;
        std::mutex _route_lock;

        std::set<std::shared_ptr<C2ServerEndpoint>> _connections;
        //Map pubkey fingerprint to connection object
        using connection_lookup_type = Common::Utils::BlobIndexedMap<std::shared_ptr<C2ServerEndpoint>>;
        connection_lookup_type _connection_lookup;
        std::mutex _connection_lock;

        //The server socket
        uint16_t _port;
        Common::Network::TCPSHandleS _socket;
        //Server event emitter
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _server_emitter_provider;
        std::shared_ptr<Common::Utils::EventEmitter> _server_emitter;
        //Client event emitter
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _client_emitter_provider;
        Common::Utils::shared_unique_ptr<Common::Utils::EventEmitter> _client_emitter;

        //Users allowed to use this server
        const std::shared_ptr<const Common::Identity::UserGroup> _authorised_users;
        const std::shared_ptr<const Common::Crypto::PrKey> _self;

        void on_accept(Common::Network::TCPCHandleU&& socket);

        void on_error(Common::ErrorCode code);

        void on_close();

        C2Server(
            uint16_t port,
            const std::shared_ptr<const Common::Identity::UserGroup> authorised_users,
            const std::shared_ptr<const Common::Crypto::PrKey> self
        );

        void start();

        ~C2Server();
    };


    class C2ServerEndpoint : public Common::Utils::SharedOnly<C2ServerEndpoint> {
        friend class Common::Utils::SharedOnly<C2ServerEndpoint>;
        //The socket (probably not needed)
        Common::Network::TCPCHandleS _socket;
        //Underlying pipes
        std::shared_ptr<Common::Transport::OPTPipes> _underlying;
        //The main server pool, for registering identity
        std::shared_ptr<C2Server> _server;
        //Iterator where this object was inserted, to be removed.
        std::set<std::shared_ptr<C2ServerEndpoint>>::iterator _self;
        C2Server::connection_lookup_type::iterator _self_lu;

        Common::Utils::Blob _identity_fingerprint;

        //Clients to broadcast login/logout to
        std::list<Common::Utils::Blob> _broadcast_clients;

        //Called when a packet can be read
        void on_open();
        //A new pipe is requested
        void on_pipe(Common::Transport::OPTPipeOpenData pipe_req);
        //Called when a packet can be read
        void on_packet(const Common::Utils::ConstBlobView& data);
        //Called when a socket error occurs
        void on_error(Common::ErrorCode code);
        //Socket was closed
        void on_close();

        C2ServerEndpoint(
            Common::Network::TCPCHandleS socket,
            std::shared_ptr<Common::Transport::OPTPipes> underlying,
            std::shared_ptr<C2Server> server);

        void start();

        void init() override;

        void close();
    };



    
    class C2ServerRoute : public Common::Utils::SharedOnly<C2ServerRoute> {
        friend class Common::Utils::SharedOnly<C2ServerRoute>;
        //The main server pool, for registering identity
        std::shared_ptr<C2Server> _server;

        //The iterator to self registration
        std::list<std::shared_ptr<C2ServerRoute>>::iterator _self;
        
        //The incoming pipe
        std::shared_ptr<Common::Transport::OPTPipe> _pipe_a;
        //The outgoing pipe
        std::shared_ptr<Common::Transport::OPTPipe> _pipe_b;

        //Called when the second half of the connection is opened
        void on_open();
        //Called when a packet can be read from a
        void on_packet_a(const Common::Utils::ConstBlobView& data);
        //Called when a packet can be read from b
        void on_packet_b(const Common::Utils::ConstBlobView& data);
        
        void on_error(Common::ErrorCode code);

        void on_close_a();
        void on_close_b();

        C2ServerRoute(
            std::shared_ptr<C2Server> server,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            std::shared_ptr<Common::Transport::OPTPipe> remote_pipe
            );
    public:
        static std::shared_ptr<C2ServerRoute> create(
            std::shared_ptr<C2Server> server,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            std::shared_ptr<Common::Transport::OPTPipe> remote_pipe
            );

        void start(const Common::Utils::ConstBlobView& data);

        void close();

        ~C2ServerRoute();
    };    
};