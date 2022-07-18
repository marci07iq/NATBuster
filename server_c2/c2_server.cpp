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

    void C2ServerRoute::start() {
        {
            std::lock_guard _lg(_server->_route_lock);
            _self = _server->_routes.insert(_server->_routes.end(), shared_from_this());
        }

        _pipe_b->set_open_callback(new Common::Utils::MemberWCallback<C2ServerRoute, void>(weak_from_this(), &C2ServerRoute::on_open));
        _pipe_a->set_packet_callback(new Common::Utils::MemberWCallback<C2ServerRoute, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2ServerRoute::on_packet_a));
        _pipe_b->set_packet_callback(new Common::Utils::MemberWCallback<C2ServerRoute, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2ServerRoute::on_packet_b));
        //_underlying->set_raw_callback(new Common::Utils::MemberCallback<IPServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPServerEndpoint::on_raw));
        //_underlying->set_error_callback(new Common::Utils::MemberCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_error));
        _pipe_a->set_close_callback(new Common::Utils::MemberWCallback<C2ServerRoute, void>(weak_from_this(), &C2ServerRoute::on_close_a));
        _pipe_b->set_close_callback(new Common::Utils::MemberWCallback<C2ServerRoute, void>(weak_from_this(), &C2ServerRoute::on_close_b));

        //Set the timeout delay
        //_underlying->addDelay(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_timeout), 100000000000);

        _pipe_b->start();
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
        Common::Crypto::Hash hasher = Common::Crypto::Hash(Common::Crypto::HashAlgo::SHA256);
        if (!user->key.fingerprint(hasher, _identity_fingerprint)) {
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
                const Common::Utils::ConstBlobSliceView fingerprint = pipe_req.content.cslice_right(1);
                if (fingerprint.size() == 32) {
                    {
                        std::lock_guard _lg(_server->_connection_lock);
                        
                        //Find if the requested client is connected
                        C2Server::connection_lookup_type::iterator other = _server->_connection_lookup.find(fingerprint);
                        if (other != _server->_connection_lookup.end()) {
                            //Request a new pipe to the other
                            std::shared_ptr<Common::Transport::OPTPipe> other_pipe = other->second->_underlying->openPipe();
                            
                        }
                    }
                }
            }
        }
    }
    //Called when a packet can be read
    void C2ServerEndpoint::on_packet(const Common::Utils::ConstBlobView& data) {

    }
    //Called when a packet can be read
    /*void on_raw(const Common::Utils::ConstBlobView& data) {

    }*/
    //Called when a socket error occurs
    /*void C2ServerEndpoint::on_error() {

    }*/
    //Timer
    //Socket was closed
    void C2ServerEndpoint::on_close() {
        {
            std::lock_guard _lg(_server->_connection_lock);
            _server->_connection_lookup.erase(_self_lu);
            _server->_connections.erase(_self);
        }
    }

    C2ServerEndpoint::C2ServerEndpoint(
        Common::Network::TCPCHandle socket,
        std::shared_ptr<Common::Transport::OPTPipes> underlying,
        std::shared_ptr<C2Server> server) :
        _socket(socket), _underlying(underlying), _server(server) {

    }

    void C2ServerEndpoint::start() {
        {
            std::lock_guard _lg(_server->_connection_lock);
            _self = _server->_connections.insert(shared_from_this()).first;
        }

        _underlying->set_open_callback(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_open));
        _underlying->set_packet_callback(new Common::Utils::MemberWCallback<C2ServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2ServerEndpoint::on_packet));
        //_underlying->set_raw_callback(new Common::Utils::MemberCallback<IPServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPServerEndpoint::on_raw));
        //_underlying->set_error_callback(new Common::Utils::MemberCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_error));
        _underlying->set_close_callback(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_close));

        //Set the timeout delay
        //_underlying->addDelay(new Common::Utils::MemberWCallback<C2ServerEndpoint, void>(weak_from_this(), &C2ServerEndpoint::on_timeout), 100000000000);

        _underlying->start();
    }

    std::shared_ptr<C2ServerEndpoint> C2ServerEndpoint::create(
        Common::Network::TCPCHandle socket,
        std::shared_ptr<Common::Transport::OPTPipes> underlying,
        std::shared_ptr<C2Server> server) {
        std::shared_ptr<C2ServerEndpoint> res = std::shared_ptr<C2ServerEndpoint>(new C2ServerEndpoint(socket, underlying, server));
        res->start();
        return res;
    }




    void C2Server::connect_callback(Common::Utils::Void data) {
        std::cout << "CONNECT AVAIL" << std::endl;
        Common::Network::TCPCHandle client = _hwnd->accept();
        if (client) {
            std::cout << "ACCEPTED FROM " << client->get_remote_addr().get_addr() << ":" << client->get_remote_addr().get_port() << std::endl;

            //Create OPT TCP
            Common::Transport::OPTTCPHandle opt = Common::Transport::OPTTCP::create(false, client);
            //Create OPT Session
            Common::Crypto::PKey self_copy;
            self_copy.copy_private_from(_self);
            std::shared_ptr<Common::Transport::OPTSession> session = std::make_shared<Common::Transport::OPTSession>(false, opt, std::move(self_copy), _authorised_users);
            std::shared_ptr<Common::Transport::OPTPipes> pipes = Common::Transport::OPTPipes::create(false, session);

            std::shared_ptr<C2ServerEndpoint> endpoint = C2ServerEndpoint::create(client, pipes, shared_from_this());
        }

    }

    void C2Server::error_callback() {
        std::cout << "ERR" << std::endl;
    }

    void C2Server::close_callback() {
        std::cout << "CLOSE" << std::endl;
    }

    C2Server::C2Server(
        uint16_t port,
        std::shared_ptr<Common::Identity::UserGroup> authorised_users,
        Common::Crypto::PKey&& self
    ) :
        _hwnd(std::make_shared< Common::Network::TCPS>("", port)),
        _emitter(std::make_shared<Common::Network::TCPSEmitter>(_hwnd)),
        _authorised_users(authorised_users),
        _self(std::move(self)) {

    }

    void C2Server::start() {
        _emitter->set_result_callback(new Common::Utils::MemberWCallback<C2Server, void, Common::Utils::Void>(weak_from_this(), &C2Server::connect_callback));
        _emitter->set_error_callback(new Common::Utils::MemberWCallback<C2Server, void>(weak_from_this(), &C2Server::error_callback));
        _emitter->set_close_callback(new Common::Utils::MemberWCallback<C2Server, void>(weak_from_this(), &C2Server::close_callback));

        _emitter->start();
    }

    C2Server::~C2Server() {
        std::cout << "ENDING" << std::endl;
    }
};