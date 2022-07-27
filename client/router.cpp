#include "router.h"

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
                RouterTCPRoute::create_server(port, pipe_req.pipe, shared_from_this());
            }
        }
    }
    void Router::on_packet(const Common::Utils::ConstBlobView& data) {

    }

    void Router::on_error() {

    }
    void Router::on_close() {
        
    }


    Router::Router(
        std::shared_ptr<Common::Transport::OPTPipes> underlying,
        uint16_t remote_port,
        Common::Network::TCPSHandle tcp_server_socket
    ) : _underlying(underlying),
        _remote_port(remote_port),
        _tcp_server_socket(tcp_server_socket),
        _tcp_server_emitter(tcp_server_socket) {
    }

    void Router::start() {

    }

    std::shared_ptr<Router> Router::create(
        std::shared_ptr<Common::Transport::OPTPipes> underlying,
        uint16_t remote_port,
        Common::Network::TCPSHandle tcp_server_socket) {
        std::shared_ptr<Router> res = std::shared_ptr<Router>(new Router(underlying, remote_port, tcp_server_socket));
        res->start();
        return res;
    }

    void RouterTCPRoute::on_socket_open() {
        //Local connection accepted: Open pipe
        _pipe->start();
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
        _emitter.close();
    }
    void RouterTCPRoute::on_socket_close() {
        _pipe->close();
    }

    RouterTCPRoute::RouterTCPRoute(
        uint16_t port,
        std::shared_ptr<Common::Transport::OPTPipe> pipe,
        std::shared_ptr<Router> router) :
        _pipe(pipe),
        _router(router),
        _socket(std::make_shared<Common::Network::TCPC>("localhost", port)),
        _emitter(_socket)
    {


    }

    void RouterTCPRoute::start() {
        {
            std::lock_guard _lg(_router->_route_lock);
            _self = _router->_open_tcp.insert(_router->_open_tcp.end(), shared_from_this());
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
        uint16_t port,
        std::shared_ptr<Common::Transport::OPTPipe> pipe,
        std::shared_ptr<Router> router) {
        std::shared_ptr< RouterTCPRoute> res = std::shared_ptr< RouterTCPRoute>(new RouterTCPRoute(port, pipe, router));
        res->start();
        return res;
    }

    void RouterTCPRoute::close() {
        {
            std::lock_guard _lg(_router->_route_lock);
            if (_self != _router->_open_tcp.end()) {
                _router->_open_tcp.erase(_self);
                _self = _router->_open_tcp.end();
            }
        }

        _emitter.close();
        _pipe->close();
    }
}