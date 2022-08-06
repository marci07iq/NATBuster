#pragma once


#include "../common/transport/opt_tcp.h"
#include "../common/transport/opt_session.h"

#include "../common/utils/waker.h"

namespace NATBuster::Client {
    class IPClient : public Common::Utils::SharedOnly<IPClient> {
        friend class Common::Utils::SharedOnly<IPClient>;
        //The underlying comms layer
        std::shared_ptr<Common::Transport::OPTBase> _underlying;
        //The raw socket
        //Common::Network::TCPCHandle _socket;

        bool _success = false;
        std::string _my_ip;
        uint16_t _my_port;
        std::mutex _data_lock;

        Common::Utils::OnetimeWaker _waker;

        void on_open();
        void on_packet(const Common::Utils::ConstBlobView& data);
        void on_error(Common::ErrorCode code);
        void on_timeout();
        void on_close();


        IPClient(
            std::string server_name,
            uint16_t port,
            std::shared_ptr<Common::Network::SocketEventEmitterProvider> provider,
            std::shared_ptr<Common::Utils::EventEmitter> emitter,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
            const std::shared_ptr<const Common::Crypto::PrKey> self);

        void start();

        void init() override;
    public:
        bool done();

        void wait();

        std::optional<std::string> get_my_ip();

        std::optional<uint16_t> get_my_port();

        bool get_success();
    };
}