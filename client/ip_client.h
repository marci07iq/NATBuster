#pragma once


#include "../common/transport/opt_tcp.h"
#include "../common/transport/session.h"

#include "../common/utils/waker.h"

namespace NATBuster::Client {
    class IPClient : public std::enable_shared_from_this<IPClient> {
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
        void on_error();
        void on_timeout();
        void on_close();


        IPClient(
            std::string server_name,
            uint16_t ip,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
            Common::Crypto::PKey&& self);

        void start();
    public:
        static std::shared_ptr< IPClient> create(
            std::string server_name,
            uint16_t ip,
            std::shared_ptr<Common::Identity::UserGroup> authorised_server,
            Common::Crypto::PKey&& self);

        bool done();

        void wait();

        std::optional<std::string> get_my_ip();

        std::optional<uint16_t> get_my_port();

        bool get_success();
    };
}