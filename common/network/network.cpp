#include "network.h"


namespace NATBuster::Common::Network {
    SocketEventHandle::SocketEventHandle() {

    }
    SocketEventHandle::SocketEventHandle(SocketOSHandle&& socket) : SocketBase(std::move(socket)) {

    }

    NetworkAddress& TCPC::get_remote() {
        return _remote_address;
    }
}