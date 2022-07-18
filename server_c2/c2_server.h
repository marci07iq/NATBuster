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



    class C2Server : public std::enable_shared_from_this<C2Server> {
    public:
        std::list<std::shared_ptr<C2ServerRoute>> _routes;
        std::mutex _route_lock;

        std::set<std::shared_ptr<C2ServerEndpoint>> _connections;
        //Map pubkey fingerprint to connection object
        using connection_lookup_type = Common::Utils::BlobIndexedMap<std::shared_ptr<C2ServerEndpoint>>;
        connection_lookup_type _connection_lookup;
        std::mutex _connection_lock;

        //The server socket
        Common::Network::TCPSHandle _hwnd;
        //The evnt emitter on the server
        std::shared_ptr<Common::Network::TCPSEmitter> _emitter;

        //Users allowed to use this server
        std::shared_ptr<Common::Identity::UserGroup> _authorised_users;
        Common::Crypto::PKey _self;

        void connect_callback(Common::Utils::Void data);

        void error_callback();

        void close_callback();

        C2Server(
            uint16_t port,
            std::shared_ptr<Common::Identity::UserGroup> authorised_users,
            Common::Crypto::PKey&& self
        );

        void start();

        ~C2Server();
    };


    class C2ServerEndpoint : public std::enable_shared_from_this<C2ServerEndpoint> {
        //The socket (probably not needed)
        Common::Network::TCPCHandle _socket;
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
        //Called when a packet can be read
        /*void on_raw(const Common::Utils::ConstBlobView& data) {

        }
        //Called when a socket error occurs
        void on_error() {

        }*/
        //Socket was closed
        void on_close();

        C2ServerEndpoint(
            Common::Network::TCPCHandle socket,
            std::shared_ptr<Common::Transport::OPTPipes> underlying,
            std::shared_ptr<C2Server> server);

        void start();
    public:
        static std::shared_ptr<C2ServerEndpoint> create(
            Common::Network::TCPCHandle socket,
            std::shared_ptr<Common::Transport::OPTPipes> underlying,
            std::shared_ptr<C2Server> server);
    };



    
    class C2ServerRoute : public std::enable_shared_from_this<C2ServerRoute> {
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
        
        void on_close_a();
        void on_close_b();

        C2ServerRoute(
            std::shared_ptr<C2Server> server,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            std::shared_ptr<Common::Transport::OPTPipe> remote_pipe
            );

        void start();

        void close();
    public:
        static std::shared_ptr<C2ServerRoute> create(
            std::shared_ptr<C2Server> server,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            std::shared_ptr<Common::Transport::OPTPipe> remote_pipe
            );

        ~C2ServerRoute();
    };    
};