#include "c2_client.h"

#include "../common/utils/hex.h"

namespace NATBuster::Client {
    void C2Client::on_open() {

    }
    void C2Client::on_pipe(Common::Transport::OPTPipeOpenData pipe_req) {
        //Has a request content type
        if (1 <= pipe_req.content.size()) {
            const Common::Transport::C2PipeRequest::packet_decoder* header = Common::Transport::C2PipeRequest::packet_decoder::cview(pipe_req.content);
            //Pipe going to another user
            if (header->type == Common::Transport::C2PipeRequest::packet_decoder::PIPE_USER) {
                //Destination fingerprint
                const Common::Utils::ConstBlobSliceView src_fingerprint = pipe_req.content.cslice_right(1);

                /*std::cout << "Pipe request from ";
                Common::Utils::print_hex(src_fingerprint.cslice_left(8));
                std::cout << std::endl;*/

                std::shared_ptr<Common::Identity::User> user = _authorised_clients->findUser(src_fingerprint);

                if (user) {
                    std::cout << "Pipe request from " << *user << std::endl;

                    Common::Crypto::PKey self_key;
                    self_key.copy_private_from(_self);
                    std::shared_ptr<Common::Identity::UserGroup> trusted_users = std::make_shared<Common::Identity::UserGroup>();
                    trusted_users->addUser(user);
                    std::shared_ptr<Common::Transport::OPTSession> session = Common::Transport::OPTSession::create(false, pipe_req.pipe, std::move(self_key), trusted_users);

                    Common::Crypto::PKey self_key2;
                    self_key2.copy_private_from(_self);
                    std::shared_ptr<Puncher> puncher = Puncher::create(shared_from_this(), false, std::move(self_key2), trusted_users, session);

                    puncher->set_punch_callback(new Common::Utils::MemberWCallback<C2Client, void, std::shared_ptr<Common::Transport::OPTSession>>(weak_from_this(), &C2Client::on_punch));

                    std::cout << "Request accepted" << std::endl;
                    puncher->start();

                    //Accet the pipe from a known user
                    //pipe_req.pipe->start();
                }
            }
        }
    }
    void C2Client::on_packet(const Common::Utils::ConstBlobView& data) {

    }
    void C2Client::on_error(Common::ErrorCode code) {

    }
    void C2Client::on_close() {

    }

    void C2Client::on_punch(std::shared_ptr<Common::Transport::OPTSession> punched_session) {
        std::shared_ptr<Common::Transport::OPTPipes> pipes = Common::Transport::OPTPipes::create(punched_session->is_client(), punched_session);
        std::shared_ptr<Router> router = Router::create(shared_from_this(), pipes);
    }

    C2Client::C2Client(
        std::string server_name,
        uint16_t ip,
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> provider,
        std::shared_ptr<Common::Utils::EventEmitter> emitter,
        std::shared_ptr<Common::Identity::UserGroup> authorised_server,
        std::shared_ptr<Common::Identity::UserGroup> authorised_clients,
        Common::Crypto::PKey&& self) : _authorised_clients(authorised_clients), _self(std::move(self)) {

        std::pair<
            Common::Network::TCPCHandleU,
            Common::ErrorCode
        > socket = Common::Network::TCPC::create_connect(server_name, ip);
        Common::Network::TCPCHandleS socket_s = socket.first;

        if (socket.second == Common::ErrorCode::OK) {
            //Associate socket
            provider->associate_socket(std::move(socket.first));
            //Create OPT TCP
            Common::Transport::OPTTCPHandle opt = Common::Transport::OPTTCP::create(true, emitter, socket_s);
            //Create OPT Session
            std::shared_ptr<Common::Transport::OPTSession> session = Common::Transport::OPTSession::create(true, opt, std::move(self), authorised_server);

            _underlying = Common::Transport::OPTPipes::create(true, session);
        }
    }

    void C2Client::start() {
        _underlying->set_callback_open(new Common::Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_open));
        _underlying->set_callback_pipe(new Common::Utils::MemberWCallback<C2Client, void, Common::Transport::OPTPipeOpenData>(weak_from_this(), &C2Client::on_pipe));
        _underlying->set_callback_packet(new Common::Utils::MemberWCallback<C2Client, void, const Common::Utils::ConstBlobView&>(weak_from_this(), &C2Client::on_packet));
        _underlying->set_callback_error(new Common::Utils::MemberWCallback<C2Client, void, Common::ErrorCode>(weak_from_this(), &C2Client::on_error));
        _underlying->set_callback_close(new Common::Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_close));

        //Set the timeout delay
        //_underlying->addDelay(new Common::Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_timeout), 100000000000);

        _underlying->start();
    }

    void C2Client::init() {
        start();
    }
}