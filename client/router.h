#pragma once

#include <map>
#include <stdint.h>

#include "../common/transport/opt_pipes.h"
#include "../common/utils/event.h"
#include "../common/utils/blob.h"
#include "../common/network/network.h"
#include "../common/network/socket_pool.h"

//Terminology used in this part of the project
//Packet: Logical blocks of data
//Frame: Induvidual 

namespace NATBuster::Client {
    class RouterTCPRoute;

    class C2Client;

    class Router : public std::enable_shared_from_this<Router> {
#pragma pack(push, 1)
        //To decode the TCP forward requests
        struct request_decoder : Common::Utils::NonStack {
            uint16_t dest_port;

            //Need as non const ref, so caller must maintain ownership of Packet
            static inline request_decoder* view(Common::Utils::BlobView& packet) {
                return (request_decoder*)(packet.getw());
            }

            static inline const request_decoder* cview(const Common::Utils::ConstBlobView& packet) {
                return (const request_decoder*)(packet.getr());
            }

            ~request_decoder() = delete;
        };

        static_assert(offsetof(request_decoder, dest_port) == 0);
#pragma pack(pop)

        //The C2 client this puncher belongs to
        std::shared_ptr<C2Client> _c2_client;

        //The iterator to self registration
        std::list<std::shared_ptr<Router>>::iterator _self;

        //The underlying comms layer
        std::shared_ptr<Common::Transport::OPTPipes> _underlying;

        //The opened TCP sockets
        std::list<std::shared_ptr<RouterTCPRoute>> _open_tcp;
        std::mutex _route_lock;

        Common::Network::TCPSHandle _tcp_server_socket;
        uint16_t _remote_port;
        Common::Network::TCPSEmitter _tcp_server_emitter;

        //Collection of TCP server sockets that need to be forwareded
        /*Common::Network::SocketHWNDPool<Common::Network::TCPSHandle> _tcp_server_sockets;
        //Event emitter for TCP server sockets
        Common::Utils::PollEventEmitter<
            Common::Network::SocketHWNDPool<Common::Network::TCPSHandle>,
            Common::Network::TCPSHandle
        > _tcp_server_emitter;

        static std::list<Common::Network::TCPSHandle> create_servers(std::map<uint16_t, uint16_t> port_maps) {
            std::list<Common::Network::TCPSHandle> res;
            for
        }*/

        void on_open();
        void on_pipe(Common::Transport::OPTPipeOpenData pipe_req);
        void on_packet(const Common::Utils::ConstBlobView& data);
        void on_error();
        void on_close();


        Router(
            std::shared_ptr<C2Client> c2_client,
            std::shared_ptr<Common::Transport::OPTPipes> underlying,
            uint16_t remote_port,
            Common::Network::TCPSHandle tcp_server_socket
        );

        void start();

        friend class RouterTCPRoute;
    public:
        static std::shared_ptr<Router> create(
            std::shared_ptr<C2Client> c2_client,
            std::shared_ptr<Common::Transport::OPTPipes> underlying,
            uint16_t remote_port,
            Common::Network::TCPSHandle tcp_server_socket
        );

        /*std::shared_ptr<Common::Transport::OPTPipe> openPipe(const Common::Crypto::PKey& key) {
            Common::Utils::Blob fingerprint;
            key.fingerprint(fingerprint);
            return openPipe(fingerprint);
        }*/
    };

    class RouterTCPRoute : public std::enable_shared_from_this<RouterTCPRoute> {
        //The main router, for registering identity
        std::shared_ptr<Router> _router;

        //The iterator to self registration
        std::list<std::shared_ptr<RouterTCPRoute>>::iterator _self;

        //The underlying pipe
        std::shared_ptr<Common::Transport::OPTPipe> _pipe;

        //Client side: Accepts a TCP server connection, opens a pipe to the remote
        //Server side: Receives the pipe, forwards it to a new tcp client socket
        bool _is_client;

        Common::Network::TCPCHandle _socket;
        Common::Network::TCPCEmitter _emitter;

        void on_socket_open();
        void on_pipe_packet(const Common::Utils::ConstBlobView& data);
        void on_socket_packet(Common::Utils::Void);
        void on_error();
        void on_pipe_close();
        void on_socket_close();

        RouterTCPRoute(
            uint16_t local_port,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            std::shared_ptr<Router> router);

        RouterTCPRoute(
            Common::Network::TCPCHandle socket,
            uint16_t remote_port,
            std::shared_ptr<Router> router
            );

        void start();
    public:
        static std::shared_ptr<RouterTCPRoute> create_server(
            uint16_t local_port,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            std::shared_ptr<Router> router);

        static std::shared_ptr<RouterTCPRoute> create_client(
            Common::Network::TCPCHandle socket,
            uint16_t remote_port,
            std::shared_ptr<Router> router);

        void close();

        inline bool is_client() const {
            return _is_client;
        }
    };
}