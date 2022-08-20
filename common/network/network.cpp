#include "network.h"


namespace NATBuster::Network {
    //SocketBase

    SocketBase::SocketBase(SocketOSData* socket) noexcept : _socket(socket) {

    }

    //Extract the underlying socket
    SocketOSData* SocketBase::extract() {
        return _socket;
    }

    //SocketEventHandle

    SocketEventHandle::SocketEventHandle(Type type) : _type(type) {

    }
    SocketEventHandle::SocketEventHandle(Type type, SocketOSData* socket) :
        _type(type),
        SocketBase(socket) {
    }

    NetworkAddress& SocketEventHandle::get_remote() {
        return _remote_address;
    }

    //TCPS

    std::pair<Utils::shared_unique_ptr<TCPS>, ErrorCode>
        TCPS::create_bind(const std::string& name, uint16_t port) {

        Utils::shared_unique_ptr<TCPS> res = TCPS::create();
        ErrorCode code = res->bind(name, port);

        return std::pair<Utils::shared_unique_ptr<TCPS>, ErrorCode>
            (std::move(res), code);
    }

    //TCPC

    std::pair<Utils::shared_unique_ptr<TCPC>, ErrorCode>
        TCPC::create_resolve(const std::string& name, uint16_t port) {

        Utils::shared_unique_ptr<TCPC> res = TCPC::create();
        ErrorCode code = res->resolve(name, port);

        return std::pair<Utils::shared_unique_ptr<TCPC>, ErrorCode>
            (std::move(res), code);
    }

    //UDP
    ErrorCode UDP::bind(const std::string& name, uint16_t port) {
        NetworkAddress address;
        ErrorCode code = address.resolve(name, port);
        if (code != ErrorCode::OK) return code;
        return bind(std::move(address));
    }

    std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
        UDP::create_bind(NetworkAddress&& local) {

        Utils::shared_unique_ptr<UDP> res = UDP::create();
        ErrorCode code = res->bind(std::move(local));

        return std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
            (std::move(res), code);
    }
    std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
        UDP::create_bind(const std::string& local_name, uint16_t local_port) {

        Utils::shared_unique_ptr<UDP> res = UDP::create();
        ErrorCode code = res->bind(local_name, local_port);

        return std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
            (std::move(res), code);
    }

    void UDP::send(const Utils::ConstBlobView& data) {
        sendto(data, _remote_address);
    }


    //Set remote socket
    void UDP::set_remote(const NetworkAddress& remote) {
        _remote_address = remote;
    }
    void UDP::set_remote(NetworkAddress&& remote) {
        _remote_address = std::move(remote);
    }
    ErrorCode UDP::set_remote(const std::string& name, uint16_t port) {
        NetworkAddress address;
        ErrorCode code = address.resolve(name, port);
        if (code != ErrorCode::OK) return code;
        set_remote(std::move(address));
        return ErrorCode::OK;
    }

    //UDPMultiplexed

    void UDPMultiplexed::on_start() {
        std::lock_guard _lg(_routes_lock);
        auto it = _routes.begin();
        while (it != _routes.end()) {
            auto itold = it++;
            std::shared_ptr<UDPMultiplexedRoute> its = itold->lock();
            if (its) {
                its->_callback_start();
            }
            else {
                _routes.erase(itold);
            }
        }
        _callback_start();
    }
    void UDPMultiplexed::on_packet(const Utils::ConstBlobView& data) {
        _callback_packet(data);
    }
    void UDPMultiplexed::on_unfiltered_packet(const Utils::ConstBlobView& data, const NetworkAddress& remote_address) {
        std::lock_guard _lg(_routes_lock);
        auto it = _routes.begin();
        bool found = false;
        while (it != _routes.end()) {
            auto itold = it++;
            std::shared_ptr<UDPMultiplexedRoute> its = itold->lock();
            if (its) {
                if (its->_remote_address == remote_address) {
                    found = true;
                    its->_callback_packet(data);
                }
                its->_callback_unfiltered_packet(data, remote_address);
            }
            else {
                _routes.erase(itold);
            }
        }
        if (!found) {
            _callback_unknown_packet(data, remote_address);
        }
        _callback_unfiltered_packet(data, remote_address);
    }
    void UDPMultiplexed::on_error(ErrorCode code) {
        std::lock_guard _lg(_routes_lock);
        auto it = _routes.begin();
        while (it != _routes.end()) {
            auto itold = it++;
            std::shared_ptr<UDPMultiplexedRoute> its = itold->lock();
            if (its) {
                its->_callback_error(code);
            }
            else {
                _routes.erase(itold);
            }
        }
        _callback_error(code);
    }
    void UDPMultiplexed::on_close() {
        std::lock_guard _lg(_routes_lock);
        auto it = _routes.begin();
        while (it != _routes.end()) {
            auto itold = it++;
            std::shared_ptr<UDPMultiplexedRoute> its = itold->lock();
            if (its) {
                its->_callback_close();
            }
            else {
                _routes.erase(itold);
            }
        }
        _callback_close();
    }

    //Set remote socket
    Utils::shared_unique_ptr<UDPMultiplexedRoute> UDPMultiplexed::add_remote(const NetworkAddress& remote) {
        std::lock_guard _lg(_routes_lock);
        Utils::shared_unique_ptr<UDPMultiplexedRoute> res = UDPMultiplexedRoute::create(shared_from_this(), remote);
        _routes.push_back(res.get_shared());
        return res;
    }
    Utils::shared_unique_ptr<UDPMultiplexedRoute> UDPMultiplexed::add_remote(NetworkAddress&& remote) {
        std::lock_guard _lg(_routes_lock);
        Utils::shared_unique_ptr<UDPMultiplexedRoute> res = UDPMultiplexedRoute::create(shared_from_this(), std::move(remote));
        _routes.push_back(res.get_shared());
        return res;
    }
    Utils::shared_unique_ptr<UDPMultiplexedRoute> UDPMultiplexed::add_remote(const std::string& name, uint16_t port) {
        NetworkAddress address;
        ErrorCode code = address.resolve(name, port);
        if (code != ErrorCode::OK) return Utils::shared_unique_ptr<UDPMultiplexedRoute>();
        return add_remote(std::move(address));
    }

    void UDPMultiplexed::start() {
        _socket->set_callback_start(new Utils::MemberWCallback<UDPMultiplexed, void>(weak_from_this(), &UDPMultiplexed::on_start));
        _socket->set_callback_packet(new Utils::MemberWCallback<UDPMultiplexed, void, const Utils::ConstBlobView&>(weak_from_this(), &UDPMultiplexed::on_packet));
        _socket->set_callback_unfiltered_packet(new Utils::MemberWCallback<UDPMultiplexed, void, const Utils::ConstBlobView&, const NetworkAddress&>(weak_from_this(), &UDPMultiplexed::on_unfiltered_packet));
        _socket->set_callback_error(new Utils::MemberWCallback<UDPMultiplexed, void, ErrorCode>(weak_from_this(), &UDPMultiplexed::on_error));
        _socket->set_callback_close(new Utils::MemberWCallback<UDPMultiplexed, void>(weak_from_this(), &UDPMultiplexed::on_close));

        _socket->start();
    }
    bool UDPMultiplexed::close() {
        return _socket->close();
    }




    void UDPMultiplexedRoute::send(const Utils::ConstBlobView& data) {
        _multiplexer->_socket->sendto(data, _remote_address);
    }

    void UDPMultiplexedRoute::start() {

    }
    bool UDPMultiplexedRoute::close() {
        return false;
    }

}