#pragma once

#include <stdint.h>
#include <iostream>

namespace NATBuster {

    enum class ErrorCode : uint32_t {
        MASK_TYPE = 0xff000000,
        MASK_SYSTEM = 0x00ff0000,
        MASK_CODE = 0x0000ffff,

        //Errors resulting from a coding error (paths that should never be hit)
        //The application has a bug, and should be fixed
        TYPE_FATAL = 0x05000000,
        //The requested operation failed, and some cleanup may be neccessary
        //The application as a whole may continue
        TYPE_ERROR = 0x04000000,
        //The requested operation didn't fail, but also didn't succeed
        TYPE_WARNING = 0x03000000,
        //Information the caller may be interted in
        TYPE_INFO = 0x02000000,
        //Information the caller may be interted in when debugging
        TYPE_DEBUG = 0x01000000,

        OK = 0,

        SYSTEM_GENERIC = 0x00010000,
        //Invalid virtual function called
        GENERIC_FATAL_INVALID_VF = TYPE_FATAL | SYSTEM_GENERIC | 0x0001,

        SYSTEM_NETWORK = 0x00020000,
        //Error in the network subsystem
        NETWORK_ERROR_SYSTEM = TYPE_ERROR | SYSTEM_NETWORK | 0x0001,
        //Can't resolve a hostname
        NETWORK_ERROR_RESOLVE_HOSTNAME = TYPE_ERROR | SYSTEM_NETWORK | 0x0002,
        //Can't create socket
        NETWORK_ERROR_CREATE_SOCKET = TYPE_ERROR | SYSTEM_NETWORK | 0x0003,
        //Can't bind server socket
        NETWORK_ERROR_SERVER_BIND = TYPE_ERROR | SYSTEM_NETWORK | 0x0004,
        //Can't listen to server socket
        NETWORK_ERROR_SERVER_LISTEN = TYPE_ERROR | SYSTEM_NETWORK | 0x0005,
        //Can't connect
        NETWORK_ERROR_CONNECT = TYPE_ERROR | SYSTEM_NETWORK | 0x0006,
        //Connection started but in progress
        NETWORK_WARN_CONNECTING = TYPE_WARNING | SYSTEM_NETWORK | 0x0007,
        //Can not parse full host + port string
        NETWORK_ERROR_PARSE_FULL_HOST = TYPE_ERROR | SYSTEM_NETWORK | 0x0008,
        //Timeout
        NETWORK_ERROR_TIMEOUT = TYPE_ERROR | SYSTEM_NETWORK | 0x0009,
        //Malformed packet
        NETWORK_ERROR_MALFORMED = TYPE_ERROR | SYSTEM_NETWORK | 0x000a,

        SYSTEM_OPT = 0x00030000,
        //Invalid ping packet received
        OPT_UDP_INVALID_PING = TYPE_ERROR | SYSTEM_OPT | 0x0001,
        //Invalid pong packet received
        OPT_UDP_INVALID_PONG = TYPE_ERROR | SYSTEM_OPT | 0x0002,
        //Receive buffer overflow
        OPT_RX_OVERFLOW = TYPE_ERROR | SYSTEM_OPT | 0x0003,

        SYSTEM_CONFIG = 0x00040000,
        //Invalid key received for user
        CONFIG_INVALID_PKEY = TYPE_ERROR | SYSTEM_CONFIG | 0x0001,
    };
}


std::ostream& operator<<(std::ostream& stream, const NATBuster::ErrorCode& code);

NATBuster::ErrorCode operator&(const NATBuster::ErrorCode& lhs, const NATBuster::ErrorCode& rhs);

namespace NATBuster::ErrorCodes {
    std::string description(ErrorCode code);

    bool is_error(ErrorCode code);

    void ThrowError(ErrorCode code);
};
