#include "router.h"
#include "c2_client.h"

namespace NATBuster::Client {

    void Router::on_open() {
        std::cout << "Router open" << std::endl;
    }
    void Router::on_pipe(Common::Transport::OPTPipeOpenData pipe_req) {
        if (pipe_req.content.size() == 2) {
            const request_decoder* request_data = request_decoder::cview(pipe_req.content);
            uint16_t port = request_data->dest_port;

            bool authorised = false;
            for (auto&& it : _underlying->getUser()->ports) {
                if (it.port_min <= port && port <= it.port_max && it.dir_push && it.proto_tcp) {
                    authorised = true;
                    break;
                }
            }

            std::cout << "Pipe request to port " << port << (authorised ? " allowed" : " denied") << std::endl;

            if (authorised) {
                RouterTCPRoute::create_server(shared_from_this(), pipe_req.pipe, port);
            }
        }
    }
    void Router::on_packet(const Common::Utils::ConstBlobView& data) {

    }
    void Router::on_error(Common::ErrorCode code) {

    }
    void Router::on_close() {
        {
            std::lock_guard _lg(_c2_client->_open_routers_lock);
            if (_self != _c2_client->_open_routers.end()) {
                _c2_client->_open_routers.erase(_self);
                std::cout << "Router self erasing" << std::endl;
            }
        }
    }


    Router::Router(
        std::shared_ptr<C2Client> c2_client,
        std::shared_ptr<Common::Transport::OPTPipes> underlying
    ) : _c2_client(c2_client),
        _underlying(underlying) {
    }

    void Router::start() {
        {
            std::lock_guard _lg(_c2_client->_open_routers_lock);
            _self = _c2_client->_open_routers.insert(_c2_client->_open_routers.end(), shared_from_this());
            std::cout << "Router self registering" << std::endl;
        }

        _underlying->start();
    }

    void Router::init() {
        start();
    }

    void Router::pushPort(uint16_t local_port, uint16_t remote_port) {

    }





    void RouterTCPS::on_open() {
        //
    }
    void RouterTCPS::on_accept(Common::Network::TCPCHandleU&& socket) {
        RouterTCPRoute::create_client(_router, std::move(socket), _remote_port);
    }
    void RouterTCPS::on_error(Common::ErrorCode code) {

    }
    void RouterTCPS::on_close() {
        {
            std::lock_guard _lg(_router->_tcps_lock);
            if (_self != _router->_open_tcps.end()) {
                _router->_open_tcps.erase(_self);
                _self = _router->_open_tcps.end();
            }
        }
    }

    RouterTCPS::RouterTCPS(
        std::shared_ptr<Router> router,
        uint16_t local_port,
        uint16_t remote_port) : 
        _local_port(local_port),
        _remote_port(remote_port)
        {

    }

    void RouterTCPS::start() {
        {
            std::lock_guard _lg(_router->_tcps_lock);
            _self = _router->_open_tcps.insert(_router->_open_tcps.end(), shared_from_this());
        }

        //Create and bind server socket
        std::pair<
            Common::Utils::shared_unique_ptr<Common::Network::TCPS>,
            Common::ErrorCode
        > create_resp = Common::Network::TCPS::create_bind("", _local_port);
        if (create_resp.second != Common::ErrorCode::OK) {
            std::cout << "Can not create server socket " << (uint32_t)create_resp.second << std::endl;
            throw 1;
        }
        _tcp_server_socket = create_resp.first;

        _router->_provider->associate_socket(std::move(create_resp.first));

        _tcp_server_socket->set_callback_start(new Common::Utils::MemberWCallback<RouterTCPS, void>(weak_from_this(), &RouterTCPS::on_open));
        _tcp_server_socket->set_callback_accept(new Common::Utils::MemberWCallback<RouterTCPS, void, Common::Network::TCPCHandleU&&>(weak_from_this(), &RouterTCPS::on_accept));
        _tcp_server_socket->set_callback_error(new Common::Utils::MemberWCallback<RouterTCPS, void, Common::ErrorCode>(weak_from_this(), &RouterTCPS::on_error));
        _tcp_server_socket->set_callback_close(new Common::Utils::MemberWCallback<RouterTCPS, void>(weak_from_this(), &RouterTCPS::on_close));

        _tcp_server_socket->start();
    }

    void RouterTCPS::init() {
        start();
    }


    void RouterTCPRoute::on_socket_open() {
        if (!_is_client) {
            //Local connection accepted: Open pipe
            _pipe->start();
        }
    }
    void RouterTCPRoute::on_pipe_packet(const Common::Utils::ConstBlobView& data) {
        _socket->send(data);
    }
    void RouterTCPRoute::on_socket_packet(const Common::Utils::ConstBlobView& data) {
        _pipe->send(data);
    }
    void RouterTCPRoute::on_error(Common::ErrorCode code) {

    }
    void RouterTCPRoute::on_pipe_close() {
        {
            std::lock_guard _lg(_router->_route_lock);
            if (_self != _router->_open_routes.end()) {
                _router->_open_routes.erase(_self);
                _self = _router->_open_routes.end();
            }
        }

        _socket->close();
    }
    void RouterTCPRoute::on_socket_close() {
        {
            std::lock_guard _lg(_router->_route_lock);
            if (_self != _router->_open_routes.end()) {
                _router->_open_routes.erase(_self);
                _self = _router->_open_routes.end();
            }
        }

        _pipe->close();
    }

    RouterTCPRoute::RouterTCPRoute(
        std::shared_ptr<Router> router,
        std::shared_ptr<Common::Transport::OPTPipe> pipe,
        uint16_t local_port
    ) :
        _is_client(false),
        _pipe(pipe),
        _router(router)
    {
        //Create client socket
        Common::Network::TCPCHandleU socket = Common::Network::TCPC::create();
        Common::Network::TCPCHandleS socket_s = socket;
        _socket = socket_s;

        //Callback once socket added to thread
        auto connect_fuction = [socket_s, local_port]() {
            Common::ErrorCode res = socket_s->connect("localhost", local_port);
        };
        socket->set_callback_start(new Common::Utils::FunctionalCallback<void>(std::bind(connect_fuction)));

        //Associate socket
        _router->_provider->associate_socket(std::move(socket));

    }

    RouterTCPRoute::RouterTCPRoute(
        std::shared_ptr<Router> router,
        Common::Network::TCPCHandleU&& socket,
        uint16_t remote_port
    ) :
        _is_client(true),
        _pipe(router->_underlying->openPipe()),
        _router(router),
        _socket(socket)
    {
        
    }

    void RouterTCPRoute::start() {
        {
            std::lock_guard _lg(_router->_route_lock);
            _self = _router->_open_routes.insert(_router->_open_routes.end(), shared_from_this());
        }

        _socket->set_callback_start(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_socket_open));

        _pipe->set_callback_packet(new Common::Utils::MemberWCallback<RouterTCPRoute, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &RouterTCPRoute::on_pipe_packet));
        _socket->set_callback_packet(new Common::Utils::MemberWCallback<RouterTCPRoute, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &RouterTCPRoute::on_socket_packet));

        _pipe->set_callback_error(new Common::Utils::MemberWCallback<RouterTCPRoute, void, Common::ErrorCode>(weak_from_this(), &RouterTCPRoute::on_error));
        _socket->set_callback_error(new Common::Utils::MemberWCallback<RouterTCPRoute, void, Common::ErrorCode>(weak_from_this(), &RouterTCPRoute::on_error));

        _pipe->set_callback_close(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_pipe_close));
        _socket->set_callback_close(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_socket_close));

        _socket->start();
    }

    std::shared_ptr<RouterTCPRoute> RouterTCPRoute::create_server(
        std::shared_ptr<Router> router,
        std::shared_ptr<Common::Transport::OPTPipe> pipe,
        uint16_t local_port
    ) {
        std::shared_ptr< RouterTCPRoute> res = std::shared_ptr< RouterTCPRoute>(new RouterTCPRoute(router, pipe, local_port));
        res->start();
        return res;
    }

    std::shared_ptr<RouterTCPRoute> RouterTCPRoute::create_client(
        std::shared_ptr<Router> router,
        Common::Network::TCPCHandleU&& socket,
        uint16_t remote_port
    ) {
        std::shared_ptr< RouterTCPRoute> res = std::shared_ptr< RouterTCPRoute>(new RouterTCPRoute(router, std::move(socket), remote_port));
        res->start();
        return res;
    }

    void RouterTCPRoute::close() {
        _socket->close();
        _pipe->close();
    }
}