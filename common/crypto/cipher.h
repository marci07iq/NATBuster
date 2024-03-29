#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include <openssl/evp.h>

//static_assert(OPENSSL_VERSION_MAJOR >= 3);

#include "../utils/copy_protection.h"
#include "../utils/blob.h"
#include "../utils/math.h"

namespace NATBuster::Crypto {

    class CipherPacketStream : Utils::NonCopyable {
    protected:
        EVP_CIPHER_CTX* _ctx = nullptr;
        EVP_CIPHER* _algo = nullptr;
        CipherPacketStream(EVP_CIPHER_CTX* ctx, EVP_CIPHER* algo);
    public:
        CipherPacketStream(CipherPacketStream&& other) noexcept;
        CipherPacketStream& operator=(CipherPacketStream&& other) noexcept;

        //size of the IVs
        virtual uint32_t iv_size() = 0;
        //Set IV bytes
        virtual void set_iv(const uint8_t* bytes, uint32_t size) = 0;

        //Get key size
        virtual uint32_t key_size() = 0;
        //Set key bytes
        virtual void set_key(const uint8_t* key, uint32_t size) = 0;

        virtual uint32_t max_size_increase() = 0;

        //Size of encypted data of raw_size plaintext
        virtual uint32_t enc_size(uint32_t raw_size) = 0;
        //Size of decrypted data of enc_size ciphertext
        virtual uint32_t dec_size(uint32_t enc_size) = 0;

        virtual bool encrypt(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        ) = 0;

        virtual bool decrypt(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        ) = 0;

        virtual bool encrypt_standalone(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        ) = 0;

        virtual bool decrypt_standalone(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        ) = 0;

        virtual ~CipherPacketStream();
    };

    //Wrapper for a packet based AES-256-GCM cypher. 
    //IV usage as packet counter inspired by [RFC5647]
    //Ciphertext contains encrypted data and auth tag
    //Set up one of these classes with identical parameters on both ends
    class CipherAES256GCMPacketStream : public CipherPacketStream {
        uint8_t _key[32];

        const uint8_t _tag_size = 16;
        const uint8_t _tag_standalone_size = 10;

        uint64_t _sequential_packet_counter;
        uint64_t _standalone_packet_counter;

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
        CipherAES256GCMPacketStream();

        CipherAES256GCMPacketStream(CipherAES256GCMPacketStream&& other) noexcept;
        CipherAES256GCMPacketStream& operator=(CipherAES256GCMPacketStream&& other) noexcept;

        uint32_t iv_size();
        void set_iv(const uint8_t* bytes, uint32_t size);
        void set_iv_common(uint32_t common);
        void set_iv_packet_normal(uint64_t packet);
        void set_iv_packet_standalone(uint64_t packet);

        uint32_t key_size();
        void set_key(const uint8_t* key, uint32_t size);

        uint32_t max_size_increase();

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

        bool encrypt_standalone(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        );

        bool decrypt_standalone(
            const Utils::ConstBlobView& in,
            Utils::BlobView& out,
            const Utils::ConstBlobView& aad
        );

        ~CipherAES256GCMPacketStream();
    };
}