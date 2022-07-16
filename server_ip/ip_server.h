#pragma once

#include <set>

#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"

namespace NATBuster::Server {

    class IPServer;

    class IPServerEndpoint : public std::enable_shared_from_this<IPServerEndpoint> {
        //Underliyng OPT
        std::shared_ptr<Common::Transport::OPTBase> _underlying;
        //The socket, to get address from
        Common::Network::TCPCHandle _socket;
        //The main server pool
        std::shared_ptr<IPServer> _server;

        //Called when a packet can be read
        void on_open();
        //Called when a packet can be read
        /*void on_packet(const Common::Utils::ConstBlobView& data) {

        }
        //Called when a packet can be read
        void on_raw(const Common::Utils::ConstBlobView& data) {

        }
        //Called when a socket error occurs
        void on_error() {

        }*/
        //Timer
        void on_timeout();
        //Socket was closed
        void on_close();

        IPServerEndpoint(
            Common::Network::TCPCHandle socket,
            std::shared_ptr<Common::Transport::OPTBase> underlying,
            std::shared_ptr<IPServer> server);

        void start();
    public:
        static std::shared_ptr<IPServerEndpoint> create(
            Common::Network::TCPCHandle socket,
            std::shared_ptr<Common::Transport::OPTBase> underlying,
            std::shared_ptr<IPServer> server);
    };

    class IPServer : public std::enable_shared_from_this<IPServer> {
    public:
        std::set<std::shared_ptr<IPServerEndpoint>> _connections;
        Common::Network::TCPSHandle _hwnd;
        std::shared_ptr<Common::Network::TCPSEmitter> _emitter;
        std::shared_ptr<Common::Identity::UserGroup> _authorised_users;
        Common::Crypto::PKey _self;

        void connect_callback(Common::Utils::Void data);

        void error_callback();

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