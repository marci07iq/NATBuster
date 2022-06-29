#pragma once

#include <cstdint>
#include <random>

namespace NATBuster::Common::Random {
    inline void fast_fill(uint8_t* random, uint32_t length) {
        for (uint32_t i = 0; i < length; i++) {
            random[i] = std::rand() & 0xff;
        }
    }
};