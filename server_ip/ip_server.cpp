#include "ip_server.h"

namespace NATBuster::Server {

    //Called when a packet can be read
    void IPServerEndpoint::on_open() {
        std::string str_ip = _socket->get_remote_addr().get_addr();
        uint16_t str_port = _socket->get_remote_addr().get_port();

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
        Common::Network::TCPCHandle socket,
        std::shared_ptr<Common::Transport::OPTBase> underlying,
        std::shared_ptr<IPServer> server) :
        _socket(socket), _underlying(underlying), _server(server) {

    }

    void IPServerEndpoint::start() {
        {
            std::lock_guard _lg(_server->_connection_lock);
            _server->_connections.insert(shared_from_this());
        }

        _underlying->set_open_callback(new Common::Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_open));
        //_underlying->set_packet_callback(new Common::Utils::MemberCallback<IPServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPServerEndpoint::on_packet));
        //_underlying->set_raw_callback(new Common::Utils::MemberCallback<IPServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPServerEndpoint::on_raw));
        //_underlying->set_error_callback(new Common::Utils::MemberCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_error));
        _underlying->set_close_callback(new Common::Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_close));

        //Set the timeout delay
        _underlying->addDelay(new Common::Utils::MemberWCallback<IPServerEndpoint, void>(weak_from_this(), &IPServerEndpoint::on_timeout), 100000000000);

        _underlying->start();
    }

    std::shared_ptr<IPServerEndpoint> IPServerEndpoint::create(
        Common::Network::TCPCHandle socket,
        std::shared_ptr<Common::Transport::OPTBase> underlying,
        std::shared_ptr<IPServer> server) {
        std::shared_ptr<IPServerEndpoint> res = std::shared_ptr<IPServerEndpoint>(new IPServerEndpoint(socket, underlying, server));
        res->start();
        return res;
    }




    void IPServer::connect_callback(Common::Utils::Void data) {
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

            std::shared_ptr<IPServerEndpoint> endpoint = IPServerEndpoint::create(client, session, shared_from_this());
        }

    }

    void IPServer::error_callback() {
        std::cout << "ERR" << std::endl;
    }

    void IPServer::close_callback() {
        std::cout << "CLOSE" << std::endl;
    }

    IPServer::IPServer(
        uint16_t port,
        std::shared_ptr<Common::Identity::UserGroup> authorised_users,
        Common::Crypto::PKey&& self
    ) :
        _hwnd(std::make_shared< Common::Network::TCPS>("", port)),
        _emitter(std::make_shared<Common::Network::TCPSEmitter>(_hwnd)),
        _authorised_users(authorised_users),
        _self(std::move(self)) {

    }

    void IPServer::start() {
        _emitter->set_result_callback(new Common::Utils::MemberWCallback<IPServer, void, Common::Utils::Void>(weak_from_this(), &IPServer::connect_callback));
        _emitter->set_error_callback(new Common::Utils::MemberWCallback<IPServer, void>(weak_from_this(), &IPServer::error_callback));
        _emitter->set_close_callback(new Common::Utils::MemberWCallback<IPServer, void>(weak_from_this(), &IPServer::close_callback));

        _emitter->start();
    }

    IPServer::~IPServer() {
        std::cout << "ENDING" << std::endl;
    }
};