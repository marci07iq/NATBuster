#pragma once

#include <memory>
#include <string>
#include <stdint.h>

#include "network_win.h"

namespace NATBuster::Common::Network {
    template <typename T>
    concept SocketHwnd = requires(const T & t, const std::list<T> &elems, Time::Timeout to)
    {
        //{ (int)5 } -> std::same_as<int>;
        //Validity check. If false, the event emitter returns
        { t->valid() } -> std::same_as<bool>;
        //Close function, for when the event emitter needs to exit
        { t->close() } -> std::same_as<void>;
        //Gets the next response
        { T::element_type::find(elems, to) } -> std::same_as<Utils::PollResponse<T>>;
    };

    typedef Utils::PollEventEmitter<TCPSHandle, Utils::Void> TCPSEmitter;
    typedef Utils::PollEventEmitter<TCPCHandle, Utils::Void> TCPCEmitter;
    typedef Utils::PollEventEmitter<UDPHandle, Utils::Void> UDPEmitter;
}