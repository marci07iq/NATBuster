#include "ip_server.h"

namespace NATBuster::Endpoint {

    //Called when a packet can be read
    void IPServerEndpoint::on_open() {
        std::string str_ip = _socket->get_remote().get_addr();
        uint16_t str_port = _socket->get_remote().get_port();

        std::cout << "Sending indentity to " << str_ip << ":" << str_port << std::endl;

        Utils::Blob blob_ip = Utils::Blob::factory_string(str_ip);
        Utils::Blob blob_port = Utils::Blob::factory_empty(2);
        *((uint16_t*)(blob_port.getw())) = str_port;


        Utils::Blob blob_ip_full;
        Utils::PackedBlobWriter ip_writer(blob_ip_full);
        ip_writer.add_record(blob_ip);
        ip_writer.add_record(blob_port);

        _underlying->send(blob_ip_full);
        _underlying->close();
    }
    //Called when a packet can be read
    /*void on_packet(const Utils::ConstBlobView& data) {

    }
    //Called when a packet can be read
    void on_raw(const Utils::ConstBlobView& data) {

    }
    //Called when a socket error occurs
    void on_error() {

    }*/
    //Timer
    void IPServerEndpoint::on_timeout() {
        _underlying->close();
    }
    //Socket was closed
    void IPServerEndpoint::on_close() {
        std::lock_guard _lg(_server->_connection_lock);
        _server->_connections.erase(shared_from_this());
    }

    IPServerEndpoint::IPServerEndpoint(
        Network::TCPCHandleS socket,
        std::shared_ptr<Transport::OPTBase> underlying,
        std::shared_ptr<IPServer> server) :
        _socket(socket), _underlying(underlying), _server(server) {

    }

    void IPServerEndpoint::start() {
        {
            std::lock_guard _lg(_server->_connection_lock);
            _server->_connections.insert(shared_from_this());
        }

        _underlying->set_callback_open(new Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_open));
        _underlying->set_callback_close(new Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_close));

        //Set the timeout delay
        _underlying->add_delay(new Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_timeout), 10000000);

        _underlying->start();
    }

    void IPServerEndpoint::init() {
        start();
    }


    void IPServer::on_accept(Network::TCPCHandleU&& socket) {

        std::cout << "ACCEPTED FROM " << socket->get_remote() << std::endl;

        Network::TCPCHandleS socket_s = socket;
        _client_emitter_provider->associate_socket(std::move(socket));
        //Create OPT TCP
        Transport::OPTTCPHandle opt = Transport::OPTTCP::create(false, _client_emitter.get_shared(), socket_s);
        //Create OPT Session
        std::shared_ptr<Transport::OPTSession> session = Transport::OPTSession::create(false, opt, _self, _authorised_users);

        std::shared_ptr<IPServerEndpoint> endpoint = IPServerEndpoint::create(socket_s, session, shared_from_this());

    }

    void IPServer::on_error(ErrorCode code) {
        std::cout << code << std::endl;
    }

    void IPServer::on_close() {
        std::cout << "CLOSE" << std::endl;
    }

    IPServer::IPServer(
        uint16_t port,
        const std::shared_ptr<const Identity::UserGroup> authorised_users,
        const std::shared_ptr<const Crypto::PrKey> self
    ) :
        _port(port),
        _authorised_users(authorised_users),
        _self(self) {
    }

    void IPServer::start() {

        //Create and bind server socket
        std::pair<
            Utils::shared_unique_ptr<Network::TCPS>,
            ErrorCode
        > create_resp = Network::TCPS::create_bind("", _port);
        if (create_resp.second != ErrorCode::OK) {
            std::cout << "Can not create server socket " << (uint32_t)create_resp.second << std::endl;
            throw 1;
        }
        _socket = create_resp.first;

        //Create server emitter provider
        Utils::shared_unique_ptr<Network::SocketEventEmitterProvider> server_provider =
            Network::SocketEventEmitterProvider::create();
        _server_emitter_provider = server_provider;

        server_provider->associate_socket(std::move(create_resp.first));

        //Create client emitter provider
        Utils::shared_unique_ptr<Network::SocketEventEmitterProvider> client_provider =
            Network::SocketEventEmitterProvider::create();
        _client_emitter_provider = client_provider;

        _socket->set_callback_accept(new Utils::MemberWCallback<IPServer, void, Network::TCPCHandleU&&>(weak_from_this(), &IPServer::on_accept));
        _socket->set_callback_error(new Utils::MemberWCallback<IPServer, void, ErrorCode>(weak_from_this(), &IPServer::on_error));
        _socket->set_callback_close(new Utils::MemberWCallback<IPServer, void>(weak_from_this(), &IPServer::on_close));

        _server_emitter = Utils::EventEmitter::create();
        _server_emitter->start_async(std::move(server_provider));
        _client_emitter = Utils::EventEmitter::create();
        _client_emitter->start_async(std::move(client_provider));

        _socket->start();
    }

    IPServer::~IPServer() {
        std::cout << "ENDING" << std::endl;
    }
};