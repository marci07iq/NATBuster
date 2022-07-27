#pragma once


#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"
#include "../common/transport/opt_pipes.h"
#include "../common/transport/opt_c2.h"

#include "../common/utils/waker.h"

namespace NATBuster::Client {
    class C2Client : public std::enable_shared_from_this<C2Client> {
        //The underlying comms layer
        std::shared_ptr<Common::Transport::OPTPipes> _underlying;
        //The raw socket
        //Common::Network::TCPCHandle _socket;

        //Common::Utils::OnetimeWaker _waker;

        void on_open();
        void on_pipe(Common::Transport::OPTPipeOpenData pipe_req);
        void on_packet(const Common::Utils::ConstBlobView& data);
        void on_error();
        void on_close();


        C2Client(
            std::string server_name,
            uint16_t ip,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
            Common::Crypto::PKey&& self);

        void start();
    public:
        static std::shared_ptr<C2Client> create(
            std::string server_name,
            uint16_t ip,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
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