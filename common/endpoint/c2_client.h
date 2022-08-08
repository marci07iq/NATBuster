#pragma once

#include "../punch/punch.h"
#include "router.h"

#include "../transport/opt_tcp.h"
#include "../transport/opt_session.h"
#include "../transport/opt_pipes.h"
#include "../transport/opt_c2.h"

#include "../utils/waker.h"

namespace NATBuster::Endpoint {
    class C2Client : public Utils::SharedOnly<C2Client> {
        friend class Utils::SharedOnly<C2Client>;
        //The underlying comms layer to the C2 server
        std::shared_ptr<Transport::OPTPipes> _underlying;

        //Clients that can communicate through the C2 server
        std::shared_ptr<Identity::UserGroup> _authorised_clients;

        //Client event emitter (C2Server-C2Client)
        std::shared_ptr<Network::SocketEventEmitterProvider> _client_emitter_provider;
        std::shared_ptr<Utils::EventEmitter> _client_emitter;
        //Router event emitter
        std::shared_ptr<Network::SocketEventEmitterProvider> _router_emitter_provider;
        Utils::shared_unique_ptr<Utils::EventEmitter> _router_emitter;

        //Open punchers
        //Remove themselves after punching is complete
        std::list<std::shared_ptr<Punch::Puncher>> _open_punchers;
        std::mutex _open_punchers_lock;
        friend class Punch::Puncher;

        //Open routers
    public:
        std::list<std::shared_ptr<Router>> _open_routers;
        std::mutex _open_routers_lock;
    private:
        friend class Router;

        const std::shared_ptr<const Crypto::PrKey> _self;

        void on_open();
        //On incoming pipe from the C2 connection
        void on_pipe(Transport::OPTPipeOpenData pipe_req);
        void on_packet(const Utils::ConstBlobView& data);
        void on_error(ErrorCode code);
        void on_close();
        //On a successful punch
        void on_punch(std::shared_ptr<Transport::OPTSession> punched_session);

        C2Client(
            std::string server_name,
            uint16_t port,
            std::shared_ptr<Network::SocketEventEmitterProvider> provider,
            std::shared_ptr<Utils::EventEmitter> emitter,
            std::shared_ptr<Identity::UserGroup> authorised_server,
            std::shared_ptr<Identity::UserGroup> authorised_clients,
            const std::shared_ptr<const Crypto::PrKey> self);

        void start();

        void init() override;
    public:
        

        std::shared_ptr<Transport::OPTPipe> openPipe(const Utils::ConstBlobView& fingerprint) {
            Utils::Blob data = Utils::Blob::factory_empty(1 + fingerprint.size());

            *((uint8_t*)data.getw()) = Transport::C2PipeRequest::packet_decoder::PIPE_USER;
            data.copy_from(fingerprint, 1);

            std::shared_ptr<Transport::OPTPipe> pipe = _underlying->openPipe();
            pipe->start_client(data);
            return pipe;
        }

        std::shared_ptr<Transport::OPTPipe> openPipe(const Crypto::PuKey& key) {
            Utils::Blob fingerprint;
            key.fingerprint(fingerprint);
            return openPipe(fingerprint);
        }

        std::shared_ptr<Punch::Puncher> punch(const std::shared_ptr<const Crypto::PrKey> self, std::shared_ptr<Identity::User> remote_user) {
            std::shared_ptr<NATBuster::Transport::OPTPipe> pipe = openPipe(*remote_user->key.get());

            
            std::shared_ptr<Identity::UserGroup> remote_userg = std::make_shared<Identity::UserGroup>();
            remote_userg->addUser(remote_user);

            std::shared_ptr<Transport::OPTSession> session = Transport::OPTSession::create(true, pipe, _self, remote_userg);

            std::shared_ptr<Punch::Puncher> puncher = Punch::Puncher::create(shared_from_this(), true, self, remote_userg, session);
            puncher->set_punch_callback(new Utils::MemberWCallback<C2Client, void, std::shared_ptr<Transport::OPTSession>>(weak_from_this(), &C2Client::on_punch));

            puncher->start();

            return puncher;
        }
    };
}