#pragma once

#include <functional>

#include "../utils/blob.h"
#include "../utils/copy_protection.h"

namespace NATBuster::Common::Transport {
    typedef std::function<void(const Utils::ConstBlobView&)> OPTPacketCallback;
    typedef std::function<void(const Utils::ConstBlobView&)> OPTRawCallback;
    typedef std::function<void()> OPTErrorCallback;
    typedef std::function<void()> OPTClosedCallback;

    //Ordered packet transport
    class OPTBase : Utils::NonCopyable {
    protected:
        OPTPacketCallback _packet_callback;
        OPTRawCallback _raw_callback;
        OPTErrorCallback _error_callback;
        OPTClosedCallback _closed_callback;
        OPTBase(
            OPTPacketCallback packet_callback,
            OPTRawCallback raw_callback,
            OPTErrorCallback error_callback,
            OPTClosedCallback closed_callback);

    public:
        //Send ordered packet
        virtual void send(const Utils::ConstBlobView& packet) = 0;
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        virtual void sendRaw(const Utils::ConstBlobView& packet) = 0;
    };

    typedef std::shared_ptr<OPTBase> OPTBaseHandle;
};