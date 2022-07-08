#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "../utils/blob.h"
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

        Hash(Hash&& other) noexcept;

        Hash& operator=(Hash&& other) noexcept;

        uint32_t out_size();

        //uint8_t* out_alloc();

        bool calc(const Utils::ConstBlobView& in, Utils::BlobView& out);

        ~Hash();
    };
}