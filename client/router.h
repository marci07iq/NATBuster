#pragma once

#include <map>
#include <stdint.h>

#include "../common/transport/opt_pipes.h"
#include "../common/utils/event.h"
#include "../common/utils/blob.h"
#include "../common/network/network.h"

//Terminology used in this part of the project
//Packet: Logical blocks of data
//Frame: Induvidual 

namespace NATBuster::Client {
    class RouterTCPRoute;
    class RouterTCPS;

    class C2Client;

    class Router : public Common::Utils::SharedOnly<Router> {
        friend class Common::Utils::SharedOnly<Router>;
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

        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _provider;
        std::shared_ptr<Common::Utils::EventEmitter> _emitter;

        //The opened TCPS sockets to send to the remote side
        std::list<std::shared_ptr<RouterTCPS>> _open_tcps;
        friend class RouterTCPS;
        std::mutex _tcps_lock;

        //The opened TCP sockets
        std::list<std::shared_ptr<RouterTCPRoute>> _open_routes;
        friend class RouterTCPRoute;
        std::mutex _route_lock;

        

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
        void on_error(Common::ErrorCode code);
        void on_close();


        Router(
            std::shared_ptr<C2Client> c2_client,
            std::shared_ptr<Common::Transport::OPTPipes> underlying
        );

        void start();

        void init() override;
    public:

        void pushPort(uint16_t local_port, uint16_t remote_port);

        /*std::shared_ptr<Common::Transport::OPTPipe> openPipe(const Common::Crypto::PKey& key) {
            Common::Utils::Blob fingerprint;
            key.fingerprint(fingerprint);
            return openPipe(fingerprint);
        }*/
    };

    class RouterTCPS : public Common::Utils::SharedOnly<RouterTCPS> {
        friend class Common::Utils::SharedOnly<RouterTCPS>;
        std::shared_ptr<Router> _router;

        //The iterator to self registration
        std::list<std::shared_ptr<RouterTCPS>>::iterator _self;

        uint16_t _local_port;
        Common::Network::TCPSHandleS _tcp_server_socket;

        uint16_t _remote_port;

        void on_open();
        void on_accept(Common::Network::TCPCHandleU&& socket);
        void on_error(Common::ErrorCode code);
        void on_close();

        RouterTCPS(
            std::shared_ptr<Router> router,
            uint16_t local_port,
            uint16_t remote_port);

        void start();

        void init() override;
    };


    class RouterTCPRoute : public Common::Utils::SharedOnly<RouterTCPRoute> {
        friend class Common::Utils::SharedOnly<RouterTCPRoute>;
        //Hide the norma�l create function
        using Common::Utils::SharedOnly<RouterTCPRoute>::create;

        //The main router, for registering identity
        std::shared_ptr<Router> _router;

        //The iterator to self registration
        std::list<std::shared_ptr<RouterTCPRoute>>::iterator _self;

        //The underlying pipe
        std::shared_ptr<Common::Transport::OPTPipe> _pipe;

        //Client side: Accepts a TCP server connection, opens a pipe to the remote
        //Server side: Receives the pipe, forwards it to a new tcp client socket
        bool _is_client;

        Common::Network::TCPCHandleS _socket;

        void on_socket_open();
        void on_pipe_open();
        void on_pipe_packet(const Common::Utils::ConstBlobView& data);
        void on_socket_packet(const Common::Utils::ConstBlobView& data);
        void on_error(Common::ErrorCode code);
        void on_pipe_close();
        void on_socket_close();

        RouterTCPRoute(
            std::shared_ptr<Router> router,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            uint16_t local_port
            );

        RouterTCPRoute(
            std::shared_ptr<Router> router,
            Common::Network::TCPCHandleU&& socket
            );

        void start_server();

        void start_client(uint16_t remote_port);
    public:
        //Incoming pipe -> open local TCP connection
        //When TCP opened -> accept pipe
        static std::shared_ptr<RouterTCPRoute> create_server(
            std::shared_ptr<Router> router,
            std::shared_ptr<Common::Transport::OPTPipe> pipe,
            uint16_t local_port
        );

        //Incoming socket -> open pipe request
        //Pipe opened -> add TCP socket to event emitter
        //Ideally we would only accept the TCP client socket at this stage, but this is not possible in linux, only windows
        static std::shared_ptr<RouterTCPRoute> create_client(
            std::shared_ptr<Router> router,
            Common::Network::TCPCHandleU&& socket,
            uint16_t remote_port
            );

        void close();

        inline bool is_client() const {
            return _is_client;
        }
    };
}