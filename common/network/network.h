#pragma once

#include <iostream>
#include <stdint.h>
#include <string>

#include "../error/codes.h"
#include "../utils/blob.h"
#include "../utils/callbacks.h"

namespace NATBuster::Common::Network {
    //Network address abstract representation
    class NetworkAddressImpl;
    class NetworkAddress {
    public:
        std::unique_ptr<NetworkAddressImpl>_impl;

        
    };

    //An OS dependant socket implementation, used for 
    class SocketOSEvent;

    class SocketEventEmitter {
    public:
        using OpenCallback = Utils::Callback<>;
        using PacketCallback = Utils::Callback<const Utils::ConstBlobView&>;
        using AcceptCallback = Utils::Callback<std::unique_ptr<SocketOSEvent>>;
        using ErrorCallback = Utils::Callback<>;
        using CloseCallback = Utils::Callback<>;

    private:

        std::unique_ptr<SocketOSEvent> _socket_event;

    public:
    };
}