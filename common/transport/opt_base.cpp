#include "opt_base.h"


namespace NATBuster::Common::Transport {
    OPTBase::OPTBase(
        OPTPacketCallback packet_callback,
        OPTRawCallback raw_callback,
        OPTErrorCallback error_callback,
        OPTClosedCallback closed_callback) :
        _packet_callback(packet_callback),
        _raw_callback(raw_callback),
        _error_callback(error_callback),
        _closed_callback(closed_callback) {

    }
}