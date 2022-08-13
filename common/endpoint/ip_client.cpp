#include "ip_client.h"

namespace NATBuster::Endpoint {
    void IPClient::on_open() {

    }
    void IPClient::on_packet(const Utils::ConstBlobView& data) {
        Utils::PackedBlobReader reader(data);

        Utils::ConstBlobSliceView ip;
        Utils::ConstBlobSliceView port;
        if (!reader.next_record(ip)) {
            _underlying->close();
            return;
        }
        if (!reader.next_record(port)) {
            _underlying->close();
            return;
        }
        if (!reader.eof()) {
            _underlying->close();
            return;
        }
        if (port.size() != 2) {
            _underlying->close();
            return;
        }

        {
            std::lock_guard _lg(_data_lock);
            _my_ip = std::string((char*)ip.getr(), ip.size());
            _my_port = *(uint16_t*)port.getr();

            _success = true;
        }

        _underlying->close();
    }
    void IPClient::on_error(ErrorCode code) {
        std::cout << "Error: " << code << std::endl;
    }
    void IPClient::on_timeout() {
        _underlying->close();
    }
    void IPClient::on_close() {
        _waker.wake();
    }


    IPClient::IPClient(
        std::string server_name,
        uint16_t port,
        std::shared_ptr<Network::SocketEventEmitterProvider> provider,
        std::shared_ptr<Utils::EventEmitter> emitter,
        std::shared_ptr<Identity::UserGroup> authorised_server,
        const std::shared_ptr<const Crypto::PrKey> self) {

        //Create client socket
        Network::TCPCHandleU socket = Network::TCPC::create();
        Network::TCPCHandleS socket_s = socket;

        ErrorCode res = socket_s->resolve(server_name, port);
        if (res != ErrorCode::OK) {
            std::cout << "Could not resolve hostname" << std::endl;
        }

        //socket->set_callback_start(new Utils::FunctionalCallback<void>(std::bind(connect_fuction)));
        
        //Associate socket
        provider->associate_socket(std::move(socket));

        //Create OPT TCP
        Transport::OPTTCPHandle opt = Transport::OPTTCP::create(true, emitter, socket_s);
        //Create OPT Session
        _underlying = Transport::OPTSession::create(true, opt, self, authorised_server);

    }

    void IPClient::start() {
        _underlying->set_callback_open(new Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_open));
        _underlying->set_callback_packet(new Utils::MemberWCallback<IPClient, void, const Utils::ConstBlobView&>(weak_from_this(), &IPClient::on_packet));
        _underlying->set_callback_error(new Utils::MemberWCallback<IPClient, void, ErrorCode>(weak_from_this(), &IPClient::on_error));
        _underlying->set_callback_close(new Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_close));

        //Set the timeout delay
        _underlying->add_delay(new Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_timeout), 10000000);

        //Start the socket
        _underlying->start();
    }

    void IPClient::init() {
        start();
    }

    bool IPClient::done() {
        return _waker.done();
    }

    void IPClient::wait() {
        _waker.wait();
    }

    std::optional<std::string> IPClient::get_my_ip() {
        std::lock_guard _lg(_data_lock);

        if (_success) {
            return _my_ip;
        }
        return std::nullopt;
    }

    std::optional<uint16_t> IPClient::get_my_port() {
        std::lock_guard _lg(_data_lock);

        if (_success) {
            return _my_port;
        }
        return std::nullopt;
    }

    bool IPClient::get_success() {
        std::lock_guard _lg(_data_lock);

        return _success;
    }
}