#include "punch_sym_sym.h"


namespace NATBuster::Client {

    void HolepunchSym::sync_launch_static(std::shared_ptr<HolepunchSym> obj) {
        obj->sync_launch();
    }

    HolepunchSym::HolepunchSym(
        const std::string& remote,
        Common::Utils::Blob&& magic_ob,
        Common::Utils::Blob&& magic_ib,
        HolepunchSymSettings settings
    ) :
        _magic_ob(std::move(magic_ob)),
        _magic_ib(std::move(magic_ib)),
        _remote(remote),
        _settings(settings)
    {
        //Sensible limits
        _settings.rate = std::min(std::max(1.f, _settings.rate), 100.f);
        _settings.port_min = std::max((uint16_t)1024, _settings.port_min);
        _settings.port_max = std::max(_settings.port_min, _settings.port_max);

        //Clamp the start port inside the range (unless 0 meaning random)
        if (_settings.port_start != 0) {
            _settings.port_start = std::min(std::max(_settings.port_min, _settings.port_start), _settings.port_max);
        }
    }

    std::shared_ptr<HolepunchSym> HolepunchSym::create(
        const std::string& remote,
        Common::Utils::Blob&& magic_ob,
        Common::Utils::Blob&& magic_ib,
        HolepunchSymSettings settings
    ) {
        return std::shared_ptr<HolepunchSym>(new HolepunchSym(remote, std::move(magic_ob), std::move(magic_ib), settings));
    }

    void HolepunchSym::sync_launch() {
        Common::Time::time_type_us lifetime = Common::Time::now() + _settings.timeout;

        Common::Time::time_type_us next_socket = Common::Time::now();

        std::list<std::list<Common::Network::UDPHandle>> socket_buckets;

        uint16_t port_range_len = _settings.port_max - _settings.port_min + (uint16_t)1;
        uint16_t port_count = std::min(_settings.num_ports, port_range_len);

        std::vector<uint16_t> ports(port_range_len);
        if (_settings.port_start == 0) {
            //Fill array with all numbers
            for (size_t i = 0; i < ports.size(); i++) {
                ports[i] = i + _settings.port_min;
            }
            auto rng = std::default_random_engine{};
            std::shuffle(std::begin(ports), std::end(ports), rng);
        }
        else {
            for (size_t i = 0; i < ports.size(); i++) {
                ports[i] = (i + _settings.port_start - _settings.port_min) % port_range_len + _settings.port_min;
            }
        }

        size_t used_ports = 0;

        while (Common::Time::now() < lifetime) {
            //Check all existing socket buckets
            for (auto& sockets : socket_buckets) {
                //Check every socket in there, till there is nothimg more to find
                while (true) {
                    {
                        auto it = sockets.begin();

                        while (it != sockets.end()) {
                            if ((*it)->valid()) {
                                ++it;
                            }
                            else {
                                auto it2 = it++;
                                sockets.erase(it2);
                            }
                        }
                    }

                    Common::Utils::PollResponse<Common::Network::UDPHandle> select = Common::Network::UDP::find(sockets, 0);

                    if (select.ok()) {
                        Common::Network::UDPHandle socket = select.get();
                        Common::Utils::Blob data;
                        Common::Network::NetworkAddress src;
                        bool read_status = socket->read(data, src, _magic_ib.size() + 1, false);
                        if (read_status) {
                            //Got the magic value, this is the correct socket
                            if (data == _magic_ib) {
                                std::cout << "Holepunch successful from " << src.get_addr() << ":" << src.get_port() << std::endl;
                                socket->replaceRemote(src);

                                std::lock_guard _lg(_data_lock);
                                _socket = socket;
                                //If we received the remote magic, it is likely that their socket was opened later
                                //So they never got our magic. Send it again
                                _socket->send(_magic_ob);
                                goto done;
                            }
                        }
                        else {
                            break;
                        }
                    }
                    //Nothing left in this bucket, proceed to next
                    else {
                        break;
                    }
                }
            }

            //Spawn a new socket, if any ports left
            if (used_ports < port_count) {
                //Must have at least one bucket
                if (socket_buckets.size() == 0) {
                    socket_buckets.push_back(std::list<Common::Network::UDPHandle>());
                }
                //Make sure bucket isn't full
                if (Common::Network::SELECT_MAX <= (socket_buckets.back().size() + 2)) {
                    socket_buckets.push_back(std::list<Common::Network::UDPHandle>());
                }

                //Create new socket
                Common::Network::UDPHandle new_socket = std::make_shared<Common::Network::UDP>(_remote, ports[used_ports]);
                if (new_socket->valid()) {
                    ++used_ports;
                    socket_buckets.back().push_back(new_socket);
                    new_socket->send(_magic_ob);
                }
            }

            //Wait rate-limiting period
            next_socket = next_socket + (Common::Time::time_type_us)(1000000.f / _settings.rate);
            Common::Time::time_type_us now = Common::Time::now();
            if (now < next_socket) {
                std::this_thread::sleep_for(std::chrono::microseconds(next_socket - now));
            }
        }

    done:
        _waker.wake();
    }

    void HolepunchSym::async_launch() {
        _thread = std::thread(HolepunchSym::sync_launch_static, shared_from_this());
    }

    bool HolepunchSym::done() {
        return _waker.done();
    }

    void HolepunchSym::wait() {
        _waker.wait();
    }

    Common::Network::UDPHandle HolepunchSym::get_socket() {
        std::lock_guard _lg(_data_lock);
        return _socket;
    }
}