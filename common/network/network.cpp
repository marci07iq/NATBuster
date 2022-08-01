#include "network.h"


namespace NATBuster::Common::Network {
    //SocketBase

    SocketBase::SocketBase() {

    }
    SocketBase::SocketBase(SocketOSHandle&& socket) noexcept : _socket(std::move(socket)) {

    }
    void SocketBase::set(SocketOSHandle&& socket) {
        _socket = std::move(socket);
    }


    //Extract the underlying socket
    SocketOSHandle SocketBase::extract() {
        return std::move(_socket);
    }

    //SocketEventHandle

    SocketEventHandle::SocketEventHandle(Type type) : _type(type) {

    }
    SocketEventHandle::SocketEventHandle(Type type, SocketOSHandle&& socket) :
        _type(type),
        SocketBase(std::move(socket)) {
    }

    NetworkAddress& SocketEventHandle::get_remote() {
        return _remote_address;
    }
}