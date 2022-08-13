#include "ip_server_udp.h"

namespace NATBuster::Endpoint {
    void IPServerUDP::on_start() {
        std::cout << "Server started" << std::endl;
    }
    void IPServerUDP::on_unfiltered_packet(const Utils::ConstBlobView& data, const Network::NetworkAddress& remote_address) {
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
        }
    }
    void IPServerUDP::on_error(ErrorCode code) {
        std::cout << code << std::endl;
    }
    void IPServerUDP::on_close() {
        _waker.wake();
    }

    IPServerUDP::IPServerUDP(
        uint16_t port
    ) : _port(port) {

    }

    void IPServerUDP::start() {
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

        //Create server emitter provider
        Utils::shared_unique_ptr<Network::SocketEventEmitterProvider> server_provider =
            Network::SocketEventEmitterProvider::create();
        _server_emitter_provider = server_provider;

        server_provider->associate_socket(std::move(create_resp.first));

        _socket->set_callback_start(new Utils::MemberWCallback<IPServerUDP, void>(weak_from_this(), &IPServerUDP::on_start));
        //_socket->set_callback_packet(new Utils::MemberWCallback<IPServerUDP, void, const Utils::ConstBlobView&>(weak_from_this(), &IPServerUDP::on_packet));
        _socket->set_callback_unfiltered_packet(new Utils::MemberWCallback<IPServerUDP, void, const Utils::ConstBlobView&, const Network::NetworkAddress&>(weak_from_this(), &IPServerUDP::on_unfiltered_packet));
        _socket->set_callback_error(new Utils::MemberWCallback<IPServerUDP, void, ErrorCode>(weak_from_this(), &IPServerUDP::on_error));
        _socket->set_callback_close(new Utils::MemberWCallback<IPServerUDP, void>(weak_from_this(), &IPServerUDP::on_close));

        _server_emitter = Utils::EventEmitter::create();
        _server_emitter->start_async(std::move(server_provider));
        
        _socket->start();
    }
    void IPServerUDP::stop() {
        _server_emitter->stop();
    }

    bool IPServerUDP::done() {
        return _waker.done();
    }
    void IPServerUDP::wait() {
        _waker.wait();
    }
};