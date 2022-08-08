#include <openssl/rand.h>

#include "random.h"

namespace NATBuster::Crypto {
    bool random(Utils::BlobView& out, const uint32_t len) {
        out.resize(len);
        return 1 == RAND_bytes(out.getw(), len);
    }

    bool random(uint8_t* dst, const uint32_t len) {
        return 1 == RAND_bytes(dst, len);
    }

    bool random_alloc(uint8_t*& dst, const uint32_t len) {
        dst = new uint8_t[len];
        return 1 == RAND_bytes(dst, len);
    }

    
}