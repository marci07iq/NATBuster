#include "ip_client_udp.h"

namespace NATBuster::Endpoint {
    void IPClientUDP::on_open() {
        std::lock_guard _lg(_results_lock);
        Utils::Blob out = Utils::Blob::factory_string("abcdefghijklmnopqrtuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrtuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        for (auto&& it : _queries) {
            it->send(out);
        }
    }
    void IPClientUDP::on_packet(std::weak_ptr<IPClientUDP> self, std::shared_ptr<LookupResult> result, const Utils::ConstBlobView& data) {
        std::shared_ptr<IPClientUDP> selfs = self.lock();
        if (selfs) {
            std::lock_guard _lg(selfs->_results_lock);
            if (!selfs->_done) {
                if (!result->_done) {
                    result->_done = true;
                    selfs->_wait_cnt--;
                    if (selfs->_wait_cnt == 0) {
                        selfs->_done = true;
                        selfs->_waker.wake();
                    }
                }

                Utils::PackedBlobReader reader(data);

                Utils::ConstBlobSliceView ip;
                Utils::ConstBlobSliceView port;
                if (!reader.next_record(ip)) {
                    result->_code = ErrorCode::NETWORK_ERROR_MALFORMED;
                    return;
                }
                if (!reader.next_record(port)) {
                    result->_code = ErrorCode::NETWORK_ERROR_MALFORMED;
                    return;
                }
                if (!reader.eof()) {
                    result->_code = ErrorCode::NETWORK_ERROR_MALFORMED;
                    return;
                }
                if (port.size() != 2) {
                    result->_code = ErrorCode::NETWORK_ERROR_MALFORMED;
                    return;
                }

                {
                    std::string my_ip = std::string((char*)ip.getr(), ip.size());
                    uint16_t my_port = *(uint16_t*)port.getr();

                    result->_my_address = Network::NetworkAddress();
                    result->_my_address.resolve(my_ip, my_port);
                    result->_code = ErrorCode::OK;

                }
            }
        }
    }
    /*void IPClientUDP::on_error(std::shared_ptr<Network::UDPMultiplexedRoute> route, ErrorCode code) {

    }
    void IPClientUDP::on_close(std::shared_ptr<Network::UDPMultiplexedRoute> route) {

    }*/
    void IPClientUDP::on_timeout() {
        std::lock_guard _lg(_results_lock);
        if (!_done) {
            _done = true;
            _waker.wake();
        }
    }

    IPClientUDP::IPClientUDP(
        std::list<host_data> query_addrs,
        std::shared_ptr<Network::SocketEventEmitterProvider> provider,
        std::shared_ptr<Utils::EventEmitter> emitter
        //std::shared_ptr<Identity::UserGroup> authorised_server,
        //const std::shared_ptr<const Crypto::PrKey> self
    ) : _query_addrs(query_addrs), _provider(provider), _emitter(emitter) {

    }

    void IPClientUDP::start() {

        //Create and bind server socket
        std::pair<
            Utils::shared_unique_ptr<Network::UDP>,
            ErrorCode
        > create_resp = Network::UDP::create_bind("0.0.0.0", 0);

        if (create_resp.second != ErrorCode::OK) {
            std::cout << "Can not create server socket " << (uint32_t)create_resp.second << std::endl;
            throw 1;
        }

        std::shared_ptr<Network::UDP> socket = create_resp.first;
        Utils::shared_unique_ptr<Network::UDPMultiplexed> multiplex = Network::UDPMultiplexed::create(socket);

        _provider->associate_socket(std::move(create_resp.first));

        //Set startup callback, for sending requests
        multiplex->set_callback_start(new Utils::MemberWCallback<IPClientUDP, void>(weak_from_this(), &IPClientUDP::on_open));

        for (auto&& it : _query_addrs) {
            //Create results
            std::shared_ptr<LookupResult> result = std::make_shared<LookupResult>();
            result->_local_address = socket->get_local();
            result->_remote_host = it;
            result->_done = false;
            _results.push_back(result);

            Network::NetworkAddress remote_address;
            ErrorCode code = remote_address.resolve(it.name, it.port);

            if (code == ErrorCode::OK) {
                result->_remote_address = remote_address;
                result->_code = ErrorCode::NETWORK_ERROR_TIMEOUT;

                Utils::shared_unique_ptr<Network::UDPMultiplexedRoute> query = multiplex->add_remote(remote_address);
                if (query) {
                    _queries.push_back(query.get_shared());

                    query->set_callback_packet(
                        new Utils::FunctionalCallback<void, const Utils::ConstBlobView&>(std::bind(
                            IPClientUDP::on_packet,
                            weak_from_this(), result, std::placeholders::_1)));

                    _wait_cnt++;
                }
                else {
                    result->_code = ErrorCode::NETWORK_ERROR_CONNECT;
                }
                //_socket->set_callback_unfiltered_packet(new Utils::MemberWCallback<IPServerUDP, void, const Utils::ConstBlobView&, const Network::NetworkAddress&>(weak_from_this(), &IPServerUDP::on_unfiltered_packet));
                //_socket->set_callback_error(new Utils::MemberWCallback<IPServerUDP, void, ErrorCode>(weak_from_this(), &IPServerUDP::on_error));
                //_socket->set_callback_close(new Utils::MemberWCallback<IPServerUDP, void>(weak_from_this(), &IPServerUDP::on_close));
            }
            else {
                result->_code = code;
            }
        }

        _emitter->add_delay(new Utils::MemberWCallback<IPClientUDP, void>(weak_from_this(), &IPClientUDP::on_timeout), 10000000);

        multiplex->start();
    }

    void IPClientUDP::init() {
        start();
    }

    bool IPClientUDP::done() {
        return _waker.done();
    }

    void IPClientUDP::wait() {
        _waker.wait();
    }

    std::list<std::shared_ptr<IPClientUDP::LookupResult>> IPClientUDP::get_results() {
        std::lock_guard _lg(_results_lock);
        if (_done) {
            return _results;
        }
        return std::list<std::shared_ptr<IPClientUDP::LookupResult>>();
    }
}