#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "hash.h"

namespace NATBuster::Crypto {

    struct hash_algo_params {
        const char* openssl_name;
    };

    //Same order as the HashAlgo
    const hash_algo_params HashAlgoNames[] = {
        {.openssl_name = "SHA-256"},
        {.openssl_name = "SHA-512"},
    };


    Hash::Hash(HashAlgo algo) {
        _ctx = EVP_MD_CTX_new();

        _algo = EVP_MD_fetch(nullptr, HashAlgoNames[(uint8_t)algo].openssl_name, nullptr);
    }

    Hash::Hash(Hash&& other) noexcept {
        _ctx = other._ctx;
        other._ctx = nullptr;

        _algo = other._algo;
        other._algo = nullptr;
    }

    Hash& Hash::operator=(Hash&& other) noexcept {
        if (_ctx != nullptr) EVP_MD_CTX_free(_ctx);
        _ctx = other._ctx;
        other._ctx = nullptr;

        if (_algo != nullptr) EVP_MD_free(_algo);
        _algo = other._algo;
        other._algo = nullptr;

        return *this;
    }

    uint32_t Hash::out_size() {
        return EVP_MD_get_size(_algo);
    }

    /*uint8_t* Hash::out_alloc() {
        return new uint8_t[out_size()];
    }*/

    bool Hash::calc(const Utils::ConstBlobView& in, Utils::BlobView& out) {
        /* Initialise the digest operation */
        if (!EVP_DigestInit_ex(_ctx, _algo, nullptr))
            return false;

        if (!EVP_DigestUpdate(_ctx, in.getr(), in.size()))
            return false;

        out.resize(EVP_MD_get_size(_algo));

        uint32_t write_len;
        if (!EVP_DigestFinal_ex(_ctx, out.getw(), &write_len))
            return false;

        //Check for buffer overruns
        assert(write_len == out.size());
        return write_len == out.size();
    }

    Hash::~Hash() {
        if (_algo != nullptr) {
            EVP_MD_free(_algo);
        }

        if (_ctx != nullptr) {
            EVP_MD_CTX_free(_ctx);
        }
    }
}

