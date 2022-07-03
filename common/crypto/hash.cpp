#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "hash.h"

namespace NATBuster::Common::Crypto {

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

        _algo = EVP_MD_fetch(NULL, HashAlgoNames[(uint8_t)algo].openssl_name, NULL);
    }

    uint32_t Hash::out_size() {
        return EVP_MD_get_size(_algo);
    }

    uint8_t* Hash::out_alloc() {
        return new uint8_t[out_size()];
    }

    bool Hash::calc(const uint8_t* in, uint32_t in_len, uint8_t* out, uint32_t out_len) {
        /* Initialise the digest operation */
        if (!EVP_DigestInit_ex(_ctx, _algo, NULL))
            return false;

        if (!EVP_DigestUpdate(_ctx, in, in_len))
            return false;

        uint32_t write_len;
        if (!EVP_DigestFinal_ex(_ctx, out, &write_len))
            return false;

        //Check for buffer overruns
        assert(write_len <= out_len);
        return write_len == out_len;
    }

    bool Hash::calc_alloc(const uint8_t* in, uint32_t in_len, uint8_t*& out, uint32_t& out_len) {
        /* Initialise the digest operation */
        if (!EVP_DigestInit_ex(_ctx, _algo, NULL))
            return false;

        if (!EVP_DigestUpdate(_ctx, in, in_len))
            return false;

        out_len = EVP_MD_get_size(_algo);

        out = new uint8_t[out_len];

        uint32_t write_len;
        if (!EVP_DigestFinal_ex(_ctx, out, &write_len))
            return false;

        //Check for buffer overruns
        return write_len == out_len;
    }

    Hash::~Hash() {
        if (_algo != NULL) {
            EVP_MD_free(_algo);
        }

        if (_ctx != NULL) {
            EVP_MD_CTX_free(_ctx);
        }
    }
}

