#include <cstdint>

namespace NATBuster::Common::Crypto {
    bool random(uint8_t* dst, const uint32_t len);

    bool random_alloc(uint8_t*& dst, const uint32_t len);

    inline bool random_u8(uint8_t& dst) {
        return random((uint8_t*)(&dst), sizeof(dst));
    }
    inline bool random_u16(uint16_t& dst) {
        return random((uint8_t*)(&dst), sizeof(dst));
    }
    inline bool random_u32(uint32_t& dst) {
        return random((uint8_t*)(&dst), sizeof(dst));
    }
    inline bool random_u64(uint64_t& dst) {
        return random((uint8_t*)(&dst), sizeof(dst));
    }
}