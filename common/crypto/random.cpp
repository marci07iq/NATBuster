#include <openssl/rand.h>

#include "random.h"

namespace NATBuster::Common::Crypto {
    bool random(uint8_t* dst, const uint32_t len) {
        return 1 == RAND_bytes(dst, len);
    }

    bool random_alloc(uint8_t*& dst, const uint32_t len) {
        dst = new uint8_t[len];
        return 1 == RAND_bytes(dst, len);
    }

    
}