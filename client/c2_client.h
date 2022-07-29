#pragma once

#include "punch.h"
#include "router.h"

#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"
#include "../common/transport/opt_pipes.h"
#include "../common/transport/opt_c2.h"

#include "../common/utils/waker.h"

namespace NATBuster::Client {
    class C2Client : public std::enable_shared_from_this<C2Client> {
        //The underlying comms layer to the C2 server
        std::shared_ptr<Common::Transport::OPTPipes> _underlying;

        //Clients that can communicate through the C2 server
        std::shared_ptr<Common::Identity::UserGroup> _authorised_clients;

        //Open punchers
        //Remove themselves after punching is complete
        std::list<std::shared_ptr<Puncher>> _open_punchers;
        std::mutex _open_punchers_lock;
        friend class Puncher;

        //Open routers
        std::list<std::shared_ptr<Router>> _open_routers;
        std::mutex _open_routers_lock;
        friend class Router;

        Common::Crypto::PKey self;

        void on_open();
        //On incoming pipe from the C2 connection
        void on_pipe(Common::Transport::OPTPipeOpenData pipe_req);
        void on_packet(const Common::Utils::ConstBlobView& data);
        void on_error();
        void on_close();
        //On a successful punch
        void on_punch(std::shared_ptr<Common::Transport::OPTSession> punched_session);

        C2Client(
            std::string server_name,
            uint16_t ip,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
            std::shared_ptr<Common::Identity::UserGroup> authorised_clients,
            Common::Crypto::PKey&& self);

        void start();
    public:
        static std::shared_ptr<C2Client> create(
            std::string server_name,
            uint16_t ip,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
            std::shared_ptr<Common::Identity::UserGroup> authorised_clients,
            Common::Crypto::PKey&& self);

        std::shared_ptr<Common::Transport::OPTPipe> openPipe(const Common::Utils::ConstBlobView& fingerprint) {
            Common::Utils::Blob data = Common::Utils::Blob::factory_empty(1 + fingerprint.size());

            *((uint8_t*)data.getw()) = Common::Transport::C2PipeRequest::packet_decoder::PIPE_USER;
            data.copy_from(fingerprint, 1);

            std::shared_ptr<Common::Transport::OPTPipe> pipe = _underlying->openPipe();
            pipe->start_client(data);
            return pipe;
        }

        std::shared_ptr<Common::Transport::OPTPipe> openPipe(const Common::Crypto::PKey& key) {
            Common::Utils::Blob fingerprint;
            key.fingerprint(fingerprint);
            return openPipe(fingerprint);
        }
    };
}