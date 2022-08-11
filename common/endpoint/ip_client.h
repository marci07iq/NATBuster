#pragma once


#include "../transport/opt_tcp.h"
#include "../transport/opt_session.h"

#include "../utils/waker.h"

namespace NATBuster::Endpoint {
    class IPClient : public Utils::SharedOnly<IPClient> {
        friend class Utils::SharedOnly<IPClient>;
        //The underlying comms layer
        std::shared_ptr<Transport::OPTBase> _underlying;
        //The raw socket
        //Network::TCPCHandle _socket;

        bool _success = false;
        std::string _my_ip;
        uint16_t _my_port;
        std::mutex _data_lock;

        Utils::OnetimeWaker _waker;

        void on_open();
        void on_packet(const Utils::ConstBlobView& data);
        void on_error(ErrorCode code);
        void on_timeout();
        void on_close();


        IPClient(
            std::string server_name,
            uint16_t port,
            std::shared_ptr<Network::SocketEventEmitterProvider> provider,
            std::shared_ptr<Utils::EventEmitter> emitter,
            std::shared_ptr<Identity::UserGroup> authorised_server,
            const std::shared_ptr<const Crypto::PrKey> self);

        void start();

        void init() override;
    public:
        bool done();

        void wait();

        std::optional<std::string> get_my_ip();

        std::optional<uint16_t> get_my_port();

        bool get_success();

        ~IPClient() {
            std::cout << "IPClient dtor" << std::endl;
        }
    };
}