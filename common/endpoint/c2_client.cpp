#include "c2_client.h"

#include "../utils/hex.h"

namespace NATBuster::Endpoint {
    void C2Client::on_open() {

    }
    void C2Client::on_pipe(Transport::OPTPipeOpenData pipe_req) {
        //Has a request content type
        if (1 <= pipe_req.content.size()) {
            const Transport::C2PipeRequest::packet_decoder* header = Transport::C2PipeRequest::packet_decoder::cview(pipe_req.content);
            //Pipe going to another user
            if (header->type == Transport::C2PipeRequest::packet_decoder::PIPE_USER) {
                //Destination fingerprint
                const Utils::ConstBlobSliceView src_fingerprint = pipe_req.content.cslice_right(1);

                /*std::cout << "Pipe request from ";
                Utils::print_hex(src_fingerprint.cslice_left(8));
                std::cout << std::endl;*/

                std::shared_ptr<Identity::User> user = _authorised_clients->findUser(src_fingerprint);

                if (user) {
                    std::cout << "Pipe request from " << *user << std::endl;

                    std::shared_ptr<Identity::UserGroup> trusted_users = std::make_shared<Identity::UserGroup>();
                    trusted_users->addUser(user);
                    std::shared_ptr<Transport::OPTSession> session = Transport::OPTSession::create(false, pipe_req.pipe, _self, trusted_users);

                    std::shared_ptr<Punch::Puncher> puncher = Punch::Puncher::create(shared_from_this(), false, _self, trusted_users, session);

                    puncher->set_punch_callback(new Utils::MemberWCallback<C2Client, void, std::shared_ptr<Transport::OPTSession>>(weak_from_this(), &C2Client::on_punch));

                    std::cout << "Request accepted" << std::endl;
                    puncher->start();
                }
            }
        }
    }
    void C2Client::on_packet(const Utils::ConstBlobView& data) {
        (void)data;
    }
    void C2Client::on_error(ErrorCode code) {
        std::cout << "Error: " << code << std::endl;
    }
    void C2Client::on_close() {

    }

    void C2Client::on_punch(std::shared_ptr<Transport::OPTSession> punched_session) {
        std::shared_ptr<Transport::OPTPipes> pipes = Transport::OPTPipes::create(punched_session->is_client(), punched_session);
        std::shared_ptr<Router> router = Router::create(shared_from_this(), pipes);
    }

    C2Client::C2Client(
        std::string server_name,
        uint16_t port,
        std::shared_ptr<Network::SocketEventEmitterProvider> provider,
        std::shared_ptr<Utils::EventEmitter> emitter,
        std::shared_ptr<Identity::UserGroup> authorised_server,
        std::shared_ptr<Identity::UserGroup> authorised_clients,
        const std::shared_ptr<const Crypto::PrKey> self) :
        _authorised_clients(authorised_clients),
        _self(self),
        _client_emitter(emitter),
        _client_emitter_provider(provider) {

        //Create client socket
        Network::TCPCHandleU socket = Network::TCPC::create();
        Network::TCPCHandleS socket_s = socket;

        ErrorCode res = socket_s->resolve(server_name, port);
        //TODO
        (void)res;
        
        //Associate socket
        provider->associate_socket(std::move(socket));

        //Create OPT TCP
        Transport::OPTTCPHandle opt = Transport::OPTTCP::create(true, emitter, socket_s);
        //Create OPT Session
        std::shared_ptr<Transport::OPTSession> session = Transport::OPTSession::create(true, opt, _self, authorised_server);
        _underlying = Transport::OPTPipes::create(true, session);
    }

    void C2Client::start() {
        _underlying->set_callback_open(new Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_open));
        _underlying->set_callback_pipe(new Utils::MemberWCallback<C2Client, void, Transport::OPTPipeOpenData>(weak_from_this(), &C2Client::on_pipe));
        _underlying->set_callback_packet(new Utils::MemberWCallback<C2Client, void, const Utils::ConstBlobView&>(weak_from_this(), &C2Client::on_packet));
        _underlying->set_callback_error(new Utils::MemberWCallback<C2Client, void, ErrorCode>(weak_from_this(), &C2Client::on_error));
        _underlying->set_callback_close(new Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_close));

        //Set the timeout delay
        //_underlying->addDelay(new Utils::MemberWCallback<C2Client, void>(weak_from_this(), &C2Client::on_timeout), 100000000000);

        Utils::shared_unique_ptr<Network::SocketEventEmitterProvider> router_provider =
            Network::SocketEventEmitterProvider::create();
        _router_emitter_provider = router_provider;

        _router_emitter = Utils::EventEmitter::create();
        _router_emitter->start_async(std::move(router_provider));

        _underlying->start();
    }

    void C2Client::init() {
        start();
    }
}