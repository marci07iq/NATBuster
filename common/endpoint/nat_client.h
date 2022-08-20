#pragma once

#include "../transport/opt_tcp.h"
#include "../transport/opt_session.h"

#include "../utils/waker.h"

namespace NATBuster::Endpoint {
    class NATClient : public Utils::SharedOnly<NATClient> {
        friend class Utils::SharedOnly<NATClient>;
        //The underlying comms layer
    public:
        struct host_data {
            std::string name;
            uint16_t port;
        };
    private:
        std::list<host_data> _query_addrs;
        std::list<std::shared_ptr<Network::UDPMultiplexedRoute>> _queries;

        struct LookupResult {
            //Outbound address, as seen by this machine
            Network::NetworkAddress _local_address;
            host_data _remote_host;
            //Remote address of the target test server
            Network::NetworkAddress _remote_address;
            //Our address as seen by the remote
            Network::NetworkAddress _my_address;
            //State
            bool _done = false;
            bool _foreign = false;
            ErrorCode _code = ErrorCode::NETWORK_ERROR_TIMEOUT;
        };
        std::list<std::shared_ptr<LookupResult>> _results;
        int _wait_cnt = 0;
        bool _done = false;
        std::mutex _results_lock;

        Utils::OnetimeWaker _waker;

        std::shared_ptr<Network::SocketEventEmitterProvider> _provider;
        std::shared_ptr<Utils::EventEmitter> _emitter;

        void on_open();
        void on_timeout();

        static void on_packet(std::weak_ptr<NATClient> self, std::shared_ptr<LookupResult> route, const Utils::ConstBlobView& data);
        static void on_unknown_packet(std::weak_ptr<NATClient> self, const Utils::ConstBlobView& data, const Network::NetworkAddress& addr);
        //void on_error(std::shared_ptr<Network::UDPMultiplexedRoute> route, ErrorCode code);
        //void on_close(std::shared_ptr<Network::UDPMultiplexedRoute> route);

        NATClient(
            std::list<host_data> query_addrs,
            std::shared_ptr<Network::SocketEventEmitterProvider> provider,
            std::shared_ptr<Utils::EventEmitter> emitter
            //std::shared_ptr<Identity::UserGroup> authorised_server,
            //const std::shared_ptr<const Crypto::PrKey> self
        );

        void start();

        void init() override;
    public:
        bool done();

        void wait();

        std::list<std::shared_ptr<LookupResult>> get_results();
    };
}