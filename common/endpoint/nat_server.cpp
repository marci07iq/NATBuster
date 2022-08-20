#include "nat_server.h"

namespace NATBuster::Endpoint {
    void NATServerO::on_start() {
    }
    void NATServerO::on_error(ErrorCode code) {
        std::cout << code << std::endl;
    }
    void NATServerO::on_close() {
    }

    NATServerO::NATServerO(
        std::shared_ptr<Network::SocketEventEmitterProvider> provider,
        std::shared_ptr<Utils::EventEmitter> emitter,
        uint16_t port
    ) : _provider(provider), _emitter(emitter), _port(port) {

    }

    void NATServerO::start() {
        //Create and bind server socket
        std::pair<
            Utils::shared_unique_ptr<Network::UDP>,
            ErrorCode
        > create_resp = Network::UDP::create_bind("", _port);
        if (create_resp.second != ErrorCode::OK) {
            std::cout << "Can not create server socket " << (uint32_t)create_resp.second << std::endl;
            throw 1;
        }
        _socket = create_resp.first;

        _provider->associate_socket(std::move(create_resp.first));

        _socket->set_callback_start(new Utils::MemberWCallback<NATServerO, void>(weak_from_this(), &NATServerO::on_start));
        //_socket->set_callback_packet(new Utils::MemberWCallback<NATServer, void, const Utils::ConstBlobView&>(weak_from_this(), &NATServer::on_packet));
        //_socket->set_callback_unfiltered_packet(new Utils::MemberWCallback<NATServerO, void, const Utils::ConstBlobView&, const Network::NetworkAddress&>(weak_from_this(), &NATServerO::on_unfiltered_packet));
        _socket->set_callback_error(new Utils::MemberWCallback<NATServerO, void, ErrorCode>(weak_from_this(), &NATServerO::on_error));
        _socket->set_callback_close(new Utils::MemberWCallback<NATServerO, void>(weak_from_this(), &NATServerO::on_close));

        _socket->start();
    }



    void NATServerI::on_start() {
    }
    void NATServerI::on_unfiltered_packet(const Utils::ConstBlobView& data, const Network::NetworkAddress& remote_address) {
        //Require a large inbound packet
        //To stop amplification attacks
        if (data.size() > 100) {
            std::string str_ip = remote_address.get_addr();
            uint16_t str_port = remote_address.get_port();

            std::cout << "Sending indentity to " << str_ip << ":" << str_port << std::endl;

            Utils::Blob blob_ip = Utils::Blob::factory_string(str_ip);
            Utils::Blob blob_port = Utils::Blob::factory_empty(2);
            *((uint16_t*)(blob_port.getw())) = str_port;


            Utils::Blob blob_ip_full;
            Utils::PackedBlobWriter ip_writer(blob_ip_full);
            ip_writer.add_record(blob_ip);
            ip_writer.add_record(blob_port);

            _socket->sendto(blob_ip_full, remote_address);
            _out_server->_socket->sendto(blob_ip_full, remote_address);
        }
    }
    void NATServerI::on_error(ErrorCode code) {
        std::cout << code << std::endl;
    }
    void NATServerI::on_close() {
        
    }

    NATServerI::NATServerI(
        std::shared_ptr<Network::SocketEventEmitterProvider> provider,
        std::shared_ptr<Utils::EventEmitter> emitter,
        std::shared_ptr<NATServerO> out_server,
        uint16_t port
    ) : _provider(provider), _emitter(emitter), _out_server(out_server), _port(port) {

    }

    void NATServerI::start() {
        //Create and bind server socket
        std::pair<
            Utils::shared_unique_ptr<Network::UDP>,
            ErrorCode
        > create_resp = Network::UDP::create_bind("", _port);
        if (create_resp.second != ErrorCode::OK) {
            std::cout << "Can not create server socket " << create_resp.second << std::endl;
            throw 1;
        }
        _socket = create_resp.first;

        _provider->associate_socket(std::move(create_resp.first));

        _socket->set_callback_start(new Utils::MemberWCallback<NATServerI, void>(weak_from_this(), &NATServerI::on_start));
        //_socket->set_callback_packet(new Utils::MemberWCallback<NATServer, void, const Utils::ConstBlobView&>(weak_from_this(), &NATServer::on_packet));
        _socket->set_callback_unfiltered_packet(new Utils::MemberWCallback<NATServerI, void, const Utils::ConstBlobView&, const Network::NetworkAddress&>(weak_from_this(), &NATServerI::on_unfiltered_packet));
        _socket->set_callback_error(new Utils::MemberWCallback<NATServerI, void, ErrorCode>(weak_from_this(), &NATServerI::on_error));
        _socket->set_callback_close(new Utils::MemberWCallback<NATServerI, void>(weak_from_this(), &NATServerI::on_close));

        _socket->start();
    }



    NATServer::NATServer(
        uint16_t oport,
        std::vector<uint16_t> iports
    ) : _iports(iports), _oport(oport) {

    }

    void NATServer::start() {
        //Create server emitter provider
        Utils::shared_unique_ptr<Network::SocketEventEmitterProvider> server_provider =
            Network::SocketEventEmitterProvider::create();
        _provider = server_provider;

        _emitter = Utils::EventEmitter::create();

        std::shared_ptr<NATServerO> socket_o;
        socket_o = NATServerO::create(_provider, _emitter, _oport);
        _sockets_o.push_back(socket_o);

        for (auto&& it : _iports) {
            std::shared_ptr<NATServerI> socket_i;
            socket_i = NATServerI::create(_provider, _emitter, socket_o, it);
            _sockets_i.push_back(socket_i);
        }

        _emitter->start_async(std::move(server_provider));

        for (auto&& it : _sockets_o) {
            it->start();
        }
        for (auto&& it : _sockets_i) {
            it->start();
        }
    }
    void NATServer::stop() {
        _emitter->stop();
    }

    bool NATServer::done() {
        return _waker.done();
    }
    void NATServer::wait() {
        _waker.wait();
    }
};