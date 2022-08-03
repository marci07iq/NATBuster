#include "ip_server.h"

namespace NATBuster::Server {

    //Called when a packet can be read
    void IPServerEndpoint::on_open() {
        std::string str_ip = _socket->get_remote().get_addr();
        uint16_t str_port = _socket->get_remote().get_port();

        std::cout << "Sending indentity to " << str_ip << ":" << str_port << std::endl;

        Common::Utils::Blob blob_ip = Common::Utils::Blob::factory_string(str_ip);
        Common::Utils::Blob blob_port = Common::Utils::Blob::factory_empty(2);
        *((uint16_t*)(blob_port.getw())) = str_port;


        Common::Utils::Blob blob_ip_full;
        Common::Utils::PackedBlobWriter ip_writer(blob_ip_full);
        ip_writer.add_record(blob_ip);
        ip_writer.add_record(blob_port);

        _underlying->send(blob_ip_full);
        _underlying->close();
    }
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
    void IPServerEndpoint::on_timeout() {
        _underlying->close();
    }
    //Socket was closed
    void IPServerEndpoint::on_close() {
        std::lock_guard _lg(_server->_connection_lock);
        _server->_connections.erase(shared_from_this());
    }

    IPServerEndpoint::IPServerEndpoint(
        Common::Network::TCPCHandleS socket,
        std::shared_ptr<Common::Transport::OPTBase> underlying,
        std::shared_ptr<IPServer> server) :
        _socket(socket), _underlying(underlying), _server(server) {

    }

    void IPServerEndpoint::start() {
        {
            std::lock_guard _lg(_server->_connection_lock);
            _server->_connections.insert(shared_from_this());
        }

        _underlying->set_callback_open(new Common::Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_open));
        _underlying->set_callback_close(new Common::Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_close));

        //Set the timeout delay
        _underlying->add_delay(new Common::Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_timeout), 100000000000);

        _underlying->start();
    }

    void IPServerEndpoint::init() {
        start();
    }


    void IPServer::on_accept(Common::Network::TCPCHandleU&& socket) {

        std::cout << "ACCEPTED FROM " << socket->get_remote() << std::endl;

        Common::Network::TCPCHandleS socket_s = socket;
        _client_emitter_provider->associate_socket(std::move(socket));
        //Create OPT TCP
        Common::Transport::OPTTCPHandle opt = Common::Transport::OPTTCP::create(false, _client_emitter.get_shared(), socket_s);
        //Create OPT Session
        Common::Crypto::PKey self_copy;
        self_copy.copy_private_from(_self);
        std::shared_ptr<Common::Transport::OPTSession> session = Common::Transport::OPTSession::create(false, opt, std::move(self_copy), _authorised_users);

        std::shared_ptr<IPServerEndpoint> endpoint = IPServerEndpoint::create(socket_s, session, shared_from_this());

    }

    void IPServer::on_error(Common::ErrorCode code) {
        std::cout << "ERR " << (uint32_t)code << std::endl;
    }

    void IPServer::on_close() {
        std::cout << "CLOSE" << std::endl;
    }

    IPServer::IPServer(
        uint16_t port,
        std::shared_ptr<Common::Identity::UserGroup> authorised_users,
        Common::Crypto::PKey&& self
    ) :
        _port(port),
        _authorised_users(authorised_users),
        _self(std::move(self)) {
    }

    void IPServer::start() {

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

        _socket->set_callback_accept(new Common::Utils::MemberWCallback<IPServer, void, Common::Network::TCPCHandleU&&>(weak_from_this(), &IPServer::on_accept));
        _socket->set_callback_error(new Common::Utils::MemberWCallback<IPServer, void, Common::ErrorCode>(weak_from_this(), &IPServer::on_error));
        _socket->set_callback_close(new Common::Utils::MemberWCallback<IPServer, void>(weak_from_this(), &IPServer::on_close));

        _server_emitter = Common::Utils::EventEmitter::create();
        _server_emitter->start_async(std::move(server_provider));
        _client_emitter = Common::Utils::EventEmitter::create();
        _client_emitter->start_async(std::move(client_provider));

        _socket->start();
    }

    IPServer::~IPServer() {
        std::cout << "ENDING" << std::endl;
    }
};