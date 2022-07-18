#include "c2_client.h"

namespace NATBuster::Client {
    void C2Client::on_open() {

    }
    void C2Client::on_pipe(Common::Transport::OPTPipeOpenData pipe_req) {

    }
    void C2Client::on_packet(const Common::Utils::ConstBlobView& data) {
        
    }
    void C2Client::on_error() {

    }
    void C2Client::on_close() {
        //_waker.wake();
    }


    C2Client::C2Client(
        std::string server_name,
        uint16_t ip,
        std::shared_ptr<Common::Identity::UserGroup> authorised_server,
        Common::Crypto::PKey&& self) {

        Common::Network::TCPCHandle socket = std::make_shared<Common::Network::TCPC>(server_name, ip);

        //Create OPT TCP
        Common::Transport::OPTTCPHandle opt = Common::Transport::OPTTCP::create(true, socket);
        //Create OPT Session
        std::shared_ptr<Common::Transport::OPTSession> session = std::make_shared<Common::Transport::OPTSession>(true, opt, std::move(self), authorised_server);

        _underlying = Common::Transport::OPTPipes::create(true, session);
    }

    void C2Client::start() {
        _underlying->set_open_callback(new Common::Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_open));
        _underlying->set_pipe_callback(new Common::Utils::MemberWCallback<C2Client, void, Common::Transport::OPTPipeOpenData>(weak_from_this(), &C2Client::on_pipe));
        _underlying->set_packet_callback(new Common::Utils::MemberWCallback<C2Client, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2Client::on_packet));
        //_underlying->set_raw_callback(new Common::Utils::MemberCallback<IPServerEndpoint, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &IPServerEndpoint::on_raw));
        _underlying->set_error_callback(new Common::Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_error));
        _underlying->set_close_callback(new Common::Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_close));

        //Set the timeout delay
        //_underlying->addDelay(new Common::Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_timeout), 100000000000);

        _underlying->start();
    }

    std::shared_ptr<C2Client> C2Client::create(
        std::string server_name,
        uint16_t ip,
        std::shared_ptr<Common::Identity::UserGroup> authorised_server,
        Common::Crypto::PKey&& self) {

        std::shared_ptr<C2Client> res = std::shared_ptr<C2Client>(new C2Client(server_name, ip, authorised_server, std::move(self)));
        res->start();

        return res;
    }

}