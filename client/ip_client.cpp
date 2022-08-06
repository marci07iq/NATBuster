#include "ip_client.h"

namespace NATBuster::Client {
    void IPClient::on_open() {

    }
    void IPClient::on_packet(const Common::Utils::ConstBlobView& data) {
        std::cout << "RESP" << std::endl;

        Common::Utils::PackedBlobReader reader(data);

        Common::Utils::ConstBlobSliceView ip;
        Common::Utils::ConstBlobSliceView port;
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
    void IPClient::on_error(Common::ErrorCode code) {
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
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> provider,
        std::shared_ptr<Common::Utils::EventEmitter> emitter,
        std::shared_ptr<Common::Identity::UserGroup> authorised_server,
        const std::shared_ptr<const Common::Crypto::PrKey> self) {

        //Create client socket
        Common::Network::TCPCHandleU socket = Common::Network::TCPC::create();
        Common::Network::TCPCHandleS socket_s = socket;

        Common::ErrorCode res = socket_s->resolve(server_name, port);
        if (res != Common::ErrorCode::OK) {
            std::cout << "Could not resolve hostname" << std::endl;
        }

        //socket->set_callback_start(new Common::Utils::FunctionalCallback<void>(std::bind(connect_fuction)));
        
        //Associate socket
        provider->associate_socket(std::move(socket));

        //Create OPT TCP
        Common::Transport::OPTTCPHandle opt = Common::Transport::OPTTCP::create(true, emitter, socket_s);
        //Create OPT Session
        _underlying = Common::Transport::OPTSession::create(true, opt, self, authorised_server);

    }

    void IPClient::start() {
        _underlying->set_callback_open(new Common::Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_open));
        _underlying->set_callback_packet(new Common::Utils::MemberWCallback<IPClient, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPClient::on_packet));
        _underlying->set_callback_error(new Common::Utils::MemberWCallback<IPClient, void, Common::ErrorCode>(weak_from_this(), &IPClient::on_error));
        _underlying->set_callback_close(new Common::Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_close));

        //Set the timeout delay
        _underlying->add_delay(new Common::Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_timeout), 10000000);

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