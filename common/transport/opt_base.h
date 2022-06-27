#pragma once

#include <functional>

#include "../network/network.h"
#include "../utils/copy_protection.h"

namespace NATBuster::Common::Transport {
    typedef std::function<void(Network::Packet)> OPTPacketCallback;
    typedef std::function<void(Network::Packet)> OPTRawCallback;
    typedef std::function<void()> OPTClosedCallback;

    //Ordered packet transport
    class OPTBase : Utils::NonCopyable {
        OPTPacketCallback _packet_callback;
        OPTRawCallback _raw_callback;
        OPTClosedCallback _closed_callback;
    protected:
        OPTBase(
            OPTPacketCallback packet_callback,
            OPTRawCallback raw_callback,
            OPTClosedCallback closed_callback);

    public:
        //Send ordered packet
        virtual void send(Network::Packet packet) = 0;
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        virtual void sendRaw(Network::Packet packet) = 0;
    };
};