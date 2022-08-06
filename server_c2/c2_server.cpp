#include "c2_server.h"

#include "../common/utils/hex.h"

namespace NATBuster::Server {
    void C2ServerRoute::on_open() {
        //remote pipe accepted, accept incoming pipe
        _pipe_a->start();
    }
    //Called when a packet can be read from a
    void C2ServerRoute::on_packet_a(const Common::Utils::ConstBlobView& data) {
        _pipe_b->send(data);
    }
    //Called when a packet can be read from b
    void C2ServerRoute::on_packet_b(const Common::Utils::ConstBlobView& data) {
        _pipe_a->send(data);
    }

    void C2ServerRoute::on_error(Common::ErrorCode code) {
        std::cout << (uint32_t)code << std::endl;
    }

    void C2ServerRoute::on_close_a() {
        _pipe_b->close();
    }
    void C2ServerRoute::on_close_b() {
        _pipe_a->close();
    }

    C2ServerRoute::C2ServerRoute(
        std::shared_ptr<C2Server> server,
        std::shared_ptr<Common::Transport::OPTPipe> pipe,
        std::shared_ptr<Common::Transport::OPTPipe> remote_pipe
    ) : _server(server), _pipe_a(pipe), _pipe_b(remote_pipe) {

    }

    void C2ServerRoute::start(const Common::Utils::ConstBlobView& data) {
        {
            std::lock_guard _lg(_server->_route_lock);
            _self = _server->_routes.insert(_server->_routes.end(), shared_from_this());
        }

        _pipe_b->set_callback_open(new Common::Utils::MemberWCallback<C2ServerRoute, void>(weak_from_this(), &C2ServerRoute::on_open));
        _pipe_a->set_callback_packet(new Common::Utils::MemberWCallback<C2ServerRoute, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2ServerRoute::on_packet_a));
        _pipe_b->set_callback_packet(new Common::Utils::MemberWCallback<C2ServerRoute, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2ServerRoute::on_packet_b));
        _pipe_a->set_callback_error(new Common::Utils::MemberWCallback<C2ServerRoute, void, Common::ErrorCode>(weak_from_this(), &C2ServerRoute::on_error));
        _pipe_b->set_callback_error(new Common::Utils::MemberWCallback<C2ServerRoute, void, Common::ErrorCode>(weak_from_this(), &C2ServerRoute::on_error));
        _pipe_a->set_callback_close(new Common::Utils::MemberWCallback<C2ServerRoute, void>(weak_from_this(), &C2ServerRoute::on_close_a));
        _pipe_b->set_callback_close(new Common::Utils::MemberWCallback<C2ServerRoute, void>(weak_from_this(), &C2ServerRoute::on_close_b));

        //Set the timeout delay
        //_underlying->addDelay(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_timeout), 100000000000);

        _pipe_b->start_client(data);
    }

    void C2ServerRoute::close() {
        {
            std::lock_guard _lg(_server->_route_lock);
            if (_self != _server->_routes.end()) {
                _server->_routes.erase(_self);
                _self = _server->_routes.end();
            }
        }

        _pipe_a->close();
        _pipe_b->close();
    }

    std::shared_ptr<C2ServerRoute> C2ServerRoute::create(
        std::shared_ptr<C2Server> server,
        std::shared_ptr<Common::Transport::OPTPipe> pipe,
        std::shared_ptr<Common::Transport::OPTPipe> remote_pipe
    ) {
        return std::shared_ptr<C2ServerRoute>(new C2ServerRoute(server, pipe, remote_pipe));
    }

    C2ServerRoute::~C2ServerRoute() {

    }



    //Called when a packet can be read
    void C2ServerEndpoint::on_open() {
        //Identity is now available
        //Insert into lookup table
        std::shared_ptr<Common::Identity::User> user = _underlying->getUser();
        if (!user->key->fingerprint(_identity_fingerprint)) {
            //Can't have an anon
            _underlying->close();
        }
        std::cout << "Login from client: ";
        Common::Utils::print_hex(_identity_fingerprint);
        std::cout << std::endl;

        Common::Utils::Blob fingerprint_copy;
        fingerprint_copy.copy_from(_identity_fingerprint);

        {
            std::lock_guard _lg(_server->_connection_lock);
            auto it = _server->_connection_lookup.find(fingerprint_copy);
            if (it != _server->_connection_lookup.end()) {
                std::cout << "Ejecting previous client" << std::endl;
                it->second->_self_lu = _server->_connection_lookup.end();
                it->second->close();
                _server->_connection_lookup.erase(it);
            }
            _self_lu = _server->_connection_lookup.emplace(std::move(fingerprint_copy), shared_from_this()).first;
        }

        //Send out the login packet
        //TODO
    }
    //A new pipe can be opened
    void C2ServerEndpoint::on_pipe(Common::Transport::OPTPipeOpenData pipe_req) {
        //Has a request content type
        if (1 <= pipe_req.content.size()) {
            //Deacode the header
            const Common::Transport::C2PipeRequest::packet_decoder* header = Common::Transport::C2PipeRequest::packet_decoder::cview(pipe_req.content);
            //Pipe going to another user
            if (header->type == Common::Transport::C2PipeRequest::packet_decoder::PIPE_USER) {
                //Destination fingerprint
                const Common::Utils::ConstBlobSliceView dst_fingerprint = pipe_req.content.cslice_right(1);
                //Source fingerprint
                Common::Utils::Blob src_fingerprint;
                if (pipe_req.pipe->getUser()->key->fingerprint(src_fingerprint)) {
                    std::cout << "Pipe request from " << *pipe_req.pipe->getUser() << std::endl;

                    if (dst_fingerprint.size() == 32) {
                        {
                            std::lock_guard _lg(_server->_connection_lock);

                            //Find if the requested client is connected
                            C2Server::connection_lookup_type::iterator other = _server->_connection_lookup.find(dst_fingerprint);
                            std::cout << "Pipe request dst ";
                            Common::Utils::print_hex(dst_fingerprint.cslice_left(8));
                            std::cout << std::endl;
                            if (other != _server->_connection_lookup.end()) {
                                //Create pipe open data
                                Common::Utils::Blob open_data = Common::Utils::Blob::factory_empty(1 + src_fingerprint.size());

                                *((uint8_t*)open_data.getw()) = Common::Transport::C2PipeRequest::packet_decoder::PIPE_USER;
                                open_data.copy_from(src_fingerprint, 1);

                                //Request a new pipe to the other
                                std::shared_ptr<Common::Transport::OPTPipe> other_pipe = other->second->_underlying->openPipe();

                                std::shared_ptr<C2ServerRoute> route = C2ServerRoute::create(_server, pipe_req.pipe, other_pipe);
                                route->start(open_data);

                                std::cout << "Forwareded.";
                            }
                        }
                    }
                }
            }
        }
    }
    //Called when a packet can be read
    void C2ServerEndpoint::on_packet(const Common::Utils::ConstBlobView& data) {
        (void)data;
    }
    //Called when a socket error occurs
    void C2ServerEndpoint::on_error(Common::ErrorCode code) {
        std::cout << "Error " << code << std::endl;
    }
    //Timer
    //Socket was closed
    void C2ServerEndpoint::on_close() {
        {
            std::cout << "C2ServerEndpoint self erasing" << std::endl;
            std::lock_guard _lg(_server->_connection_lock);
            if (_self_lu != _server->_connection_lookup.end()) {
                _server->_connection_lookup.erase(_self_lu);
            }
            if (_self != _server->_connections.end()) {
                _server->_connections.erase(_self);
            }
        }
    }

    C2ServerEndpoint::C2ServerEndpoint(
        Common::Network::TCPCHandleS socket,
        std::shared_ptr<Common::Transport::OPTPipes> underlying,
        std::shared_ptr<C2Server> server) :
        _socket(socket), _underlying(underlying), _server(server) {

    }

    void C2ServerEndpoint::start() {
        {
            std::lock_guard _lg(_server->_connection_lock);
            _self = _server->_connections.insert(shared_from_this()).first;
        }

        _underlying->set_callback_open(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_open));
        _underlying->set_callback_packet(new Common::Utils::MemberWCallback<C2ServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2ServerEndpoint::on_packet));
        _underlying->set_callback_pipe(new Common::Utils::MemberWCallback<C2ServerEndpoint, void, Common::Transport::OPTPipeOpenData>(weak_from_this(), &C2ServerEndpoint::on_pipe));
        _underlying->set_callback_error(new Common::Utils::MemberWCallback<C2ServerEndpoint, void, Common::ErrorCode>(weak_from_this(), &C2ServerEndpoint::on_error));
        _underlying->set_callback_close(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_close));

        //Set the timeout delay
        //_underlying->addDelay(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_timeout), 100000000000);

        _underlying->start();
    }

    void C2ServerEndpoint::init() {
        start();
    }

    void C2ServerEndpoint::close() {
        _underlying->close();
    }

    void C2Server::on_accept(Common::Network::TCPCHandleU&& socket) {
        std::cout << "ACCEPTED FROM " << socket->get_remote() << std::endl;

        Common::Network::TCPCHandleS socket_s = socket;
        _client_emitter_provider->associate_socket(std::move(socket));
        //Create OPT TCP
        Common::Transport::OPTTCPHandle opt = Common::Transport::OPTTCP::create(false, _client_emitter.get_shared(), socket_s);
        //Create OPT Session
        std::shared_ptr<Common::Transport::OPTSession> session = Common::Transport::OPTSession::create(false, opt, _self, _authorised_users);
        std::shared_ptr<Common::Transport::OPTPipes> pipes = Common::Transport::OPTPipes::create(false, session);

        std::shared_ptr<C2ServerEndpoint> endpoint = C2ServerEndpoint::create(socket_s, pipes, shared_from_this());

    }

    void C2Server::on_error(Common::ErrorCode) {
        std::cout << "ERR" << std::endl;
    }

    void C2Server::on_close() {
        std::cout << "CLOSE" << std::endl;
    }

    C2Server::C2Server(
        uint16_t port,
        const std::shared_ptr<const Common::Identity::UserGroup> authorised_users,
        const std::shared_ptr<const Common::Crypto::PrKey> self
    ) :
        _port(port),
        _authorised_users(authorised_users),
        _self(std::move(self)) {

    }

    void C2Server::start() {
        //Create and bind server socket
        std::pair<
            Common::Utils::shared_unique_ptr<Common::Network::TCPS>,
            Common::ErrorCode
        > create_resp = Common::Network::TCPS::create_bind("", _port);
        if (create_resp.second != Common::ErrorCode::OK) {
            std::cout << "Can not create server socket " << (uint32_t)create_resp.second << std::endl;
            throw 1;
        }
        _socket = create_resp.first;

        //Create server emitter provider
        Common::Utils::shared_unique_ptr<Common::Network::SocketEventEmitterProvider> server_provider =
            Common::Network::SocketEventEmitterProvider::create();
        _server_emitter_provider = server_provider;

        server_provider->associate_socket(std::move(create_resp.first));

        //Create client emitter provider
        Common::Utils::shared_unique_ptr<Common::Network::SocketEventEmitterProvider> client_provider =
            Common::Network::SocketEventEmitterProvider::create();
        _client_emitter_provider = client_provider;

        _socket->set_callback_accept(new Common::Utils::MemberWCallback<C2Server, void, Common::Network::TCPCHandleU&&>(weak_from_this(), &C2Server::on_accept));
        _socket->set_callback_error(new Common::Utils::MemberWCallback<C2Server, void, Common::ErrorCode>(weak_from_this(), &C2Server::on_error));
        _socket->set_callback_close(new Common::Utils::MemberWCallback<C2Server, void>(weak_from_this(), &C2Server::on_close));


        _server_emitter = Common::Utils::EventEmitter::create();
        _server_emitter->start_async(std::move(server_provider));
        _client_emitter = Common::Utils::EventEmitter::create();
        _client_emitter->start_async(std::move(client_provider));

        _socket->start();
    }

    C2Server::~C2Server() {
        std::cout << "ENDING" << std::endl;
    }
};