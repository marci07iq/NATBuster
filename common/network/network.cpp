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

    //Set remote socket
    ErrorCode UDP::set_remote(NetworkAddress&& remote) {
        _remote_address = std::move(remote);
        return ErrorCode::OK;
    }
    ErrorCode UDP::set_remote(const std::string& name, uint16_t port) {
        NetworkAddress address;
        ErrorCode code = address.resolve(name, port);
        if (code != ErrorCode::OK) return code;
        return set_remote(std::move(address));
    }
}