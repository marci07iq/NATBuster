#pragma once

#include <cstdint>
#include <random>

#include "../crypto/random.h"

namespace NATBuster::Common::Random {
    inline void fast_fill(uint8_t* random, uint32_t length) {
        for (uint32_t i = 0; i < length; i++) {
            random[i] = std::rand() & 0xff;
        }
    }

    inline void quality_fill(uint8_t* random, uint32_t length) {
        Crypto::random(random, length);
    }
};