#include <stdint.h>

namespace NATBuster::Common {
    enum class ErrorCode : uint32_t {
        MASK_TYPE = 0xff000000,
        MASK_SYSTEM = 0x00ff0000,
        MASK_CODE = 0x0000ffff,

        TYPE_ERROR = 0x04000000,
        TYPE_WARNING = 0x03000000,
        TYPE_INFO = 0x02000000,
        TYPE_DEBUG = 0x01000000,
        TYPE_OK = 0x00000000,

        SYSTEM_GENERIC = 0x00000000,
        OK = TYPE_OK | SYSTEM_GENERIC | 0x0000,

        SYSTEM_NETWORK = 0x00010000,
        //Error in the network subsystem
        NETWORK_ERROR_SYSTEM = TYPE_ERROR | SYSTEM_NETWORK | 0x0001,
        //Can't resolve a hostname
        NETWORK_ERROR_RESOLVE_HOSTNAME = TYPE_ERROR | SYSTEM_NETWORK | 0x0002,
    };
}