#pragma once

#include <set>

#include "../transport/opt_tcp.h"
#include "../transport/opt_session.h"
#include "../transport/opt_pipes.h"

#include "../transport/opt_c2.h"

namespace NATBuster::Endpoint {

    class C2Server;
    class C2ServerEndpoint;
    class C2ServerRoute;



    class C2Server : public Utils::SharedOnly<C2Server> {
        friend class Utils::SharedOnly<C2Server>;
    public:
        std::list<std::shared_ptr<C2ServerRoute>> _routes;
        std::mutex _route_lock;

        std::set<std::shared_ptr<C2ServerEndpoint>> _connections;
        //Map pubkey fingerprint to connection object
        using connection_lookup_type = Utils::BlobIndexedMap<std::shared_ptr<C2ServerEndpoint>>;
        connection_lookup_type _connection_lookup;
        std::mutex _connection_lock;

        //The server socket
        uint16_t _port;
        Network::TCPSHandleS _socket;
        //Server event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _server_emitter_provider;
        std::shared_ptr<Utils::EventEmitter> _server_emitter;
        //Client event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _client_emitter_provider;
        Utils::shared_unique_ptr<Utils::EventEmitter> _client_emitter;

        //Users allowed to use this server
        const std::shared_ptr<const Identity::UserGroup> _authorised_users;
        const std::shared_ptr<const Crypto::PrKey> _self;

        void on_accept(Network::TCPCHandleU&& socket);

        void on_error(ErrorCode code);

        void on_close();

        C2Server(
            uint16_t port,
            const std::shared_ptr<const Identity::UserGroup> authorised_users,
            const std::shared_ptr<const Crypto::PrKey> self
        );

        void start();

        ~C2Server();
    };


    class C2ServerEndpoint : public Utils::SharedOnly<C2ServerEndpoint> {
        friend class Utils::SharedOnly<C2ServerEndpoint>;
        //The socket (probably not needed)
        Network::TCPCHandleS _socket;
        //Underlying pipes
        std::shared_ptr<Transport::OPTPipes> _underlying;
        //The main server pool, for registering identity
        std::shared_ptr<C2Server> _server;
        //Iterator where this object was inserted, to be removed.
        std::set<std::shared_ptr<C2ServerEndpoint>>::iterator _self;
        C2Server::connection_lookup_type::iterator _self_lu;

        Utils::Blob _identity_fingerprint;

        //Clients to broadcast login/logout to
        std::list<Utils::Blob> _broadcast_clients;

        //Called when a packet can be read
        void on_open();
        //A new pipe is requested
        void on_pipe(Transport::OPTPipeOpenData pipe_req);
        //Called when a packet can be read
        void on_packet(const Utils::ConstBlobView& data);
        //Called when a socket error occurs
        void on_error(ErrorCode code);
        //Socket was closed
        void on_close();

        C2ServerEndpoint(
            Network::TCPCHandleS socket,
            std::shared_ptr<Transport::OPTPipes> underlying,
            std::shared_ptr<C2Server> server);

        void start();

        void init() override;

        void close();
    };



    
    class C2ServerRoute : public Utils::SharedOnly<C2ServerRoute> {
        friend class Utils::SharedOnly<C2ServerRoute>;
        //The main server pool, for registering identity
        std::shared_ptr<C2Server> _server;

        //The iterator to self registration
        std::list<std::shared_ptr<C2ServerRoute>>::iterator _self;
        
        //The incoming pipe
        std::shared_ptr<Transport::OPTPipe> _pipe_a;
        //The outgoing pipe
        std::shared_ptr<Transport::OPTPipe> _pipe_b;

        //Called when the second half of the connection is opened
        void on_open();
        //Called when a packet can be read from a
        void on_packet_a(const Utils::ConstBlobView& data);
        //Called when a packet can be read from b
        void on_packet_b(const Utils::ConstBlobView& data);
        
        void on_error(ErrorCode code);

        void on_close_a();
        void on_close_b();

        C2ServerRoute(
            std::shared_ptr<C2Server> server,
            std::shared_ptr<Transport::OPTPipe> pipe,
            std::shared_ptr<Transport::OPTPipe> remote_pipe
            );
    public:
        static std::shared_ptr<C2ServerRoute> create(
            std::shared_ptr<C2Server> server,
            std::shared_ptr<Transport::OPTPipe> pipe,
            std::shared_ptr<Transport::OPTPipe> remote_pipe
            );

        void start(const Utils::ConstBlobView& data);

        void close();

        ~C2ServerRoute();
    };    
};