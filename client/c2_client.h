#pragma once

#include "punch.h"
#include "router.h"

#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"
#include "../common/transport/opt_pipes.h"
#include "../common/transport/opt_c2.h"

#include "../common/utils/waker.h"

namespace NATBuster::Client {
    class C2Client : public Common::Utils::SharedOnly<C2Client> {
        friend class Common::Utils::SharedOnly<C2Client>;
        //The underlying comms layer to the C2 server
        std::shared_ptr<Common::Transport::OPTPipes> _underlying;

        //Clients that can communicate through the C2 server
        std::shared_ptr<Common::Identity::UserGroup> _authorised_clients;

        //Client event emitter (C2Server-C2Client)
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _client_emitter_provider;
        std::shared_ptr<Common::Utils::EventEmitter> _client_emitter;
        //Router event emitter
        std::shared_ptr<Common::Network::SocketEventEmitterProvider> _router_emitter_provider;
        Common::Utils::shared_unique_ptr<Common::Utils::EventEmitter> _router_emitter;

        //Open punchers
        //Remove themselves after punching is complete
        std::list<std::shared_ptr<Puncher>> _open_punchers;
        std::mutex _open_punchers_lock;
        friend class Puncher;

        //Open routers
    public:
        std::list<std::shared_ptr<Router>> _open_routers;
        std::mutex _open_routers_lock;
    private:
        friend class Router;

        const std::shared_ptr<const Common::Crypto::PrKey> _self;

        void on_open();
        //On incoming pipe from the C2 connection
        void on_pipe(Common::Transport::OPTPipeOpenData pipe_req);
        void on_packet(const Common::Utils::ConstBlobView& data);
        void on_error(Common::ErrorCode code);
        void on_close();
        //On a successful punch
        void on_punch(std::shared_ptr<Common::Transport::OPTSession> punched_session);

        C2Client(
            std::string server_name,
            uint16_t port,
            std::shared_ptr<Common::Network::SocketEventEmitterProvider> provider,
            std::shared_ptr<Common::Utils::EventEmitter> emitter,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
            std::shared_ptr<Common::Identity::UserGroup> authorised_clients,
            const std::shared_ptr<const Common::Crypto::PrKey> self);

        void start();

        void init() override;
    public:
        

        std::shared_ptr<Common::Transport::OPTPipe> openPipe(const Common::Utils::ConstBlobView& fingerprint) {
            Common::Utils::Blob data = Common::Utils::Blob::factory_empty(1 + fingerprint.size());

            *((uint8_t*)data.getw()) = Common::Transport::C2PipeRequest::packet_decoder::PIPE_USER;
            data.copy_from(fingerprint, 1);

            std::shared_ptr<Common::Transport::OPTPipe> pipe = _underlying->openPipe();
            pipe->start_client(data);
            return pipe;
        }

        std::shared_ptr<Common::Transport::OPTPipe> openPipe(const Common::Crypto::PuKey& key) {
            Common::Utils::Blob fingerprint;
            key.fingerprint(fingerprint);
            return openPipe(fingerprint);
        }

        std::shared_ptr<NATBuster::Client::Puncher> punch(const std::shared_ptr<const Common::Crypto::PrKey> self, std::shared_ptr<Common::Identity::User> remote_user) {
            std::shared_ptr<NATBuster::Common::Transport::OPTPipe> pipe = openPipe(*remote_user->key.get());

            
            std::shared_ptr<Common::Identity::UserGroup> remote_userg = std::make_shared<Common::Identity::UserGroup>();
            remote_userg->addUser(remote_user);

            std::shared_ptr<Common::Transport::OPTSession> session = Common::Transport::OPTSession::create(true, pipe, _self, remote_userg);

            std::shared_ptr<NATBuster::Client::Puncher> puncher = NATBuster::Client::Puncher::create(shared_from_this(), true, self, remote_userg, session);
            puncher->set_punch_callback(new Common::Utils::MemberWCallback<C2Client, void, std::shared_ptr<Common::Transport::OPTSession>>(weak_from_this(), &C2Client::on_punch));

            puncher->start();

            return puncher;
        }
    };
}