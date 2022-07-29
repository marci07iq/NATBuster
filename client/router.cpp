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

    void Router::on_error() {

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

    std::shared_ptr<Router> Router::create(
        std::shared_ptr<C2Client> c2_client,
        std::shared_ptr<Common::Transport::OPTPipes> underlying
    ) {
        std::shared_ptr<Router> res = std::shared_ptr<Router>(new Router(c2_client, underlying));
        res->start();
        return res;
    }

    void Router::pushPort(uint16_t local_port, uint16_t remote_port) {

    }





    void RouterTCPS::on_open() {
        //
    }
    void RouterTCPS::on_accept(Common::Utils::Void) {
        Common::Network::TCPCHandle client = _tcp_server_socket->accept();

        RouterTCPRoute::create_client(_router, client, _remote_port);
    }
    void RouterTCPS::on_error() {

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
        _tcp_server_socket(std::make_shared<Common::Network::TCPS>("", local_port)),
        _tcp_server_emitter(_tcp_server_socket),
        _remote_port(remote_port)
        {

    }

    void RouterTCPS::start() {
        {
            std::lock_guard _lg(_router->_tcps_lock);
            _self = _router->_open_tcps.insert(_router->_open_tcps.end(), shared_from_this());
        }

        _tcp_server_emitter.set_open_callback(new Common::Utils::MemberWCallback<RouterTCPS, void>(weak_from_this(), &RouterTCPS::on_open));
        _tcp_server_emitter.set_result_callback(new Common::Utils::MemberWCallback<RouterTCPS, void, Common::Utils::Void>(weak_from_this(), &RouterTCPS::on_accept));
        _tcp_server_emitter.set_error_callback(new Common::Utils::MemberWCallback<RouterTCPS, void>(weak_from_this(), &RouterTCPS::on_error));
        _tcp_server_emitter.set_close_callback(new Common::Utils::MemberWCallback<RouterTCPS, void>(weak_from_this(), &RouterTCPS::on_close));

        _tcp_server_emitter.start();
    }

    std::shared_ptr<RouterTCPS> RouterTCPS::create(
        std::shared_ptr<Router> router,
        uint16_t local_port,
        uint16_t remote_port) {
        std::shared_ptr<RouterTCPS> res = std::shared_ptr<RouterTCPS>(new RouterTCPS(router, local_port, remote_port));
        res->start();
        return res;
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
    void RouterTCPRoute::on_socket_packet(Common::Utils::Void) {
        Common::Utils::Blob packet;
        //Packet not neccessarily really available
        bool res = _socket->read(packet, 1, 8000);

        if (!res) {
            close();
            return;
        }

        _pipe->send(packet);
    }
    void RouterTCPRoute::on_error() {

    }
    void RouterTCPRoute::on_pipe_close() {
        {
            std::lock_guard _lg(_router->_route_lock);
            if (_self != _router->_open_routes.end()) {
                _router->_open_routes.erase(_self);
                _self = _router->_open_routes.end();
            }
        }

        _emitter.close();
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
        _router(router),
        _socket(std::make_shared<Common::Network::TCPC>("localhost", local_port)),
        _emitter(_socket)
    {

    }

    RouterTCPRoute::RouterTCPRoute(
        std::shared_ptr<Router> router,
        Common::Network::TCPCHandle socket,
        uint16_t remote_port
    ) :
        _is_client(true),
        _pipe(router->_underlying->openPipe()),
        _router(router),
        _socket(socket),
        _emitter(_socket)
    {

    }

    void RouterTCPRoute::start() {
        {
            std::lock_guard _lg(_router->_route_lock);
            _self = _router->_open_routes.insert(_router->_open_routes.end(), shared_from_this());
        }

        _emitter.set_open_callback(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_socket_open));

        _pipe->set_packet_callback(new Common::Utils::MemberWCallback<RouterTCPRoute, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &RouterTCPRoute::on_pipe_packet));
        _emitter.set_result_callback(new Common::Utils::MemberWCallback<RouterTCPRoute, void, Common::Utils::Void>(weak_from_this(), &RouterTCPRoute::on_socket_packet));

        _pipe->set_error_callback(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_error));
        _emitter.set_error_callback(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_error));

        _pipe->set_close_callback(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_pipe_close));
        _emitter.set_close_callback(new Common::Utils::MemberWCallback<RouterTCPRoute, void>(weak_from_this(), &RouterTCPRoute::on_socket_close));

        _emitter.start();
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
        Common::Network::TCPCHandle socket,
        uint16_t remote_port
    ) {
        std::shared_ptr< RouterTCPRoute> res = std::shared_ptr< RouterTCPRoute>(new RouterTCPRoute(router, socket, remote_port));
        res->start();
        return res;
    }

    void RouterTCPRoute::close() {
        _emitter.close();
        _pipe->close();
    }
}