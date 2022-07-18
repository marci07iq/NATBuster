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
    void IPClient::on_error() {

    }
    void IPClient::on_timeout() {
        _underlying->close();
    }
    void IPClient::on_close() {
        _waker.wake();
    }


    IPClient::IPClient(
        std::string server_name,
        uint16_t ip,
        std::shared_ptr<Common::Identity::UserGroup> authorised_server,
        Common::Crypto::PKey&& self) {

        Common::Network::TCPCHandle socket = std::make_shared<Common::Network::TCPC>(server_name, ip);

        //Create OPT TCP
        Common::Transport::OPTTCPHandle opt = Common::Transport::OPTTCP::create(true, socket);
        //Create OPT Session
        _underlying = std::make_shared<Common::Transport::OPTSession>(true, opt, std::move(self), authorised_server);
    }

    void IPClient::start() {
        _underlying->set_open_callback(new Common::Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_open));
        _underlying->set_packet_callback(new Common::Utils::MemberWCallback<IPClient, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPClient::on_packet));
        //_underlying->set_raw_callback(new Common::Utils::MemberCallback<IPServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPServerEndpoint::on_raw));
        _underlying->set_error_callback(new Common::Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_error));
        _underlying->set_close_callback(new Common::Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_close));

        //Set the timeout delay
        _underlying->addDelay(new Common::Utils::MemberWCallback<IPClient, void>(weak_from_this(), &IPClient::on_timeout), 100000000000);

        _underlying->start();
    }

    std::shared_ptr< IPClient> IPClient::create(
        std::string server_name,
        uint16_t ip,
        std::shared_ptr<Common::Identity::UserGroup> authorised_server,
        Common::Crypto::PKey&& self) {

        std::shared_ptr< IPClient> res = std::shared_ptr<IPClient>(new IPClient(server_name, ip, authorised_server, std::move(self)));
        res->start();

        return res;
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