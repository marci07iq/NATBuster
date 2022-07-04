#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include <openssl/sha.h>

#include "../utils/copy_protection.h"

namespace NATBuster::Common::Crypto {
    enum class HashAlgo : uint8_t {
        SHA256 = 0,
        SHA512 = 1
    };


    class Hash : Utils::NonCopyable {
        EVP_MD_CTX* _ctx = nullptr;
        EVP_MD* _algo = nullptr;
    public:
        Hash(HashAlgo algo);

        Hash(Hash&& other);

        Hash& operator=(Hash&& other);

        uint32_t out_size();

        uint8_t* out_alloc();

        bool calc(const uint8_t* in, const uint32_t in_len, uint8_t* out, const uint32_t out_len);
        bool calc_alloc(const uint8_t* in, const uint32_t in_len, uint8_t*& out, uint32_t& out_len);

        ~Hash();
    };
}