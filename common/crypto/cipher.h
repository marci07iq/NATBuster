#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include <openssl/evp.h>

#include "../utils/copy_protection.h"
#include "../utils/blob.h"

namespace NATBuster::Common::Crypto {
    /*enum class CipherAlgo : uint8_t {
        AES_256_GCM = 0,
        AES_256_CBC = 1
    };*/


    /*class Cipher : Utils::AbstractBase {
        EVP_CIPHER_CTX* _ctx = nullptr;
        EVP_CIPHER* _algo = nullptr;
    public:
        Cipher(CipherAlgo algo);

        virtual uint8_t iv_size();
        virtual uint32_t enc_size(uint32_t raw_size);
        virtual uint32_t dec_size(uint32_t enc_size);

        bool encrypt(const uint8_t* in, const uint32_t in_len, uint8_t* out, const uint32_t out_len);
        bool encrypt_alloc(const uint8_t* in, const uint32_t in_len, uint8_t*& out, uint32_t& out_len);

        bool decrypt(const uint8_t* in, const uint32_t in_len, uint8_t* out, const uint32_t out_len);
        bool decrypt_alloc(const uint8_t* in, const uint32_t in_len, uint8_t*& out, uint32_t& out_len);

        virtual ~Cipher();
    };*/

    //Wrapper for a packet based AES-256-GCM cypher. 
    //IV usage as packet counter inspired by [RFC5647]
    //Ciphertext contains encrypted data and auth tag
    //Set up one of these classes with identical parameters on both ends
    class CipherAES256GCMPacket : Utils::NonCopyable {
        EVP_CIPHER_CTX* _ctx = nullptr;
        EVP_CIPHER* _algo = nullptr;

        uint8_t _key[32];

#pragma pack(push, 1)
        struct {
            union {
                struct {
                    uint32_t common;
                    uint64_t packet;
                } parts;
                uint8_t bytes[12];
            };
        } _iv;
#pragma pack(pop)
        static_assert(sizeof(_iv) == 12);
        
    public:
        CipherAES256GCMPacket();

        CipherAES256GCMPacket(CipherAES256GCMPacket&& other);

        CipherAES256GCMPacket& operator=(CipherAES256GCMPacket&& other);

        uint8_t iv_size();
        void set_iv_common(uint32_t common);
        void set_iv_packet(uint64_t packet);

        uint8_t key_size();
        void set_key(uint8_t key[32]);

        uint32_t enc_size(uint32_t raw_size);
        uint32_t dec_size(uint32_t enc_size);

        bool encrypt(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        );

        bool decrypt(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        );

        ~CipherAES256GCMPacket();
    };
}