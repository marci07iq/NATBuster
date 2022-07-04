#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "cipher.h"

namespace NATBuster::Common::Crypto {

    /*struct cipher_ossl_param {
        const char* key;
        const char* value;
    };*/

    /*struct cipher_algo_params {
        const char* openssl_name;
        //cipher_ossl_param* openssl_params = nullptr;
    };

    //Same order as the CipherAlgo
    const cipher_algo_params CipherAlgoNames[] = {
        {
            .openssl_name = "AES-256-GCM",
            //.openssl_params = {}
        },
        /*{
            .openssl_name = "AES-256-CBC",
            .iv_len = 16,
            .openssl_params = {cipher_ossl_param{
                .key = OSSL_CIPHER_PARAM_CTS_MODE, .value = "CS3"
            }}
        },
    };

    Cipher::Cipher(CipherAlgo algo) {
        _ctx = EVP_CIPHER_CTX_new();

        _algo = EVP_CIPHER_fetch(nullptr, CipherAlgoNames[(uint8_t)algo].openssl_name, nullptr);
    }

    Cipher::~Cipher() {
        if (_algo != nullptr) {
            EVP_CIPHER_free(_algo);
        }

        if (_ctx != nullptr) {
            EVP_CIPHER_CTX_free(_ctx);
        }
    }*/

    CipherAES256GCMPacket::CipherAES256GCMPacket() {
        _ctx = EVP_CIPHER_CTX_new();

        _algo = EVP_CIPHER_fetch(nullptr, "AES-256-GCM", nullptr);
    }

    CipherAES256GCMPacket::CipherAES256GCMPacket(CipherAES256GCMPacket&& other) {
        _ctx = other._ctx;
        other._ctx = nullptr;

        _algo = other._algo;
        other._algo = nullptr;

        for (int i = 0; i < 32; i++) {
            _key[i] = other._key[i];
            other._key[i] = 0;
        }

        _iv.parts.common = other._iv.parts.common;
        other._iv.parts.common = 0;

        _iv.parts.packet = other._iv.parts.packet;
        other._iv.parts.packet = 0;
    }

    CipherAES256GCMPacket& CipherAES256GCMPacket::operator=(CipherAES256GCMPacket&& other) {
        if (_ctx != nullptr) EVP_CIPHER_CTX_free(_ctx);
        _ctx = other._ctx;
        other._ctx = nullptr;

        if (_algo != nullptr) EVP_CIPHER_free(_algo);
        _algo = other._algo;
        other._algo = nullptr;

        for (int i = 0; i < 32; i++) {
            _key[i] = other._key[i];
            other._key[i] = 0;
        }

        _iv.parts.common = other._iv.parts.common;
        other._iv.parts.common = 0;

        _iv.parts.packet = other._iv.parts.packet;
        other._iv.parts.packet = 0;
    }

    uint8_t CipherAES256GCMPacket::iv_size() {
        return 4;
    }
    void CipherAES256GCMPacket::set_iv_common(uint32_t common) {
        _iv.parts.common = common;
    }
    void CipherAES256GCMPacket::set_iv_packet(uint64_t packet) {
        _iv.parts.packet = packet;
    }

    uint8_t CipherAES256GCMPacket::key_size() {
        return 32;
    }
    void CipherAES256GCMPacket::set_key(uint8_t key[32]) {
        for (int i = 0; i < 32; i++) {
            _key[i] = key[i];
        }
    }

    uint32_t CipherAES256GCMPacket::enc_size(uint32_t raw_size) {
        return raw_size + 16;
    }
    uint32_t CipherAES256GCMPacket::dec_size(uint32_t enc_size) {
        return enc_size - 16;
    }

    bool CipherAES256GCMPacket::encrypt(
        const uint8_t* in, const uint32_t in_len,
        uint8_t* out, const uint32_t out_len,
        const uint8_t* aad, const uint32_t aad_len
    ) {
        assert(out_len == enc_size(in_len));

        if (1 != EVP_EncryptInit_ex(_ctx, _algo, nullptr, nullptr, nullptr))
            return false;

        if (1 != EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            return false;

        if (1 != EVP_EncryptInit_ex(_ctx, nullptr, nullptr, _key, _iv.bytes))
            return false;

        //Auth tag goes at the beginning
        int cum_len = 16;
        int len = 0;

        //Add Additional Authentication Data if needed
        if (aad != nullptr && aad_len != 0) {
            if (1 != EVP_EncryptUpdate(_ctx, nullptr, &len, aad, aad_len))
                return false;
        }

        //Encrypt data
        if (1 != EVP_EncryptUpdate(_ctx, out + cum_len, &len, in, in_len))
            return false;

        cum_len += len;
        assert(cum_len <= out_len);

        //Encrypt data (finalise)
        //For GCM, this doesnt actually produce any more bytes.
        //For block aligned, it would add padding
        if (1 != EVP_EncryptFinal_ex(_ctx, out + cum_len, &len))
            return false;

        cum_len += len;
        assert(cum_len <= out_len);

        //Get auth tag, to the beginning of out
        if (1 != EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_GET_TAG, 16, out))
            return false;

        //Reset for next packet
        EVP_CIPHER_CTX_reset(_ctx);
        _iv.parts.packet++;

        return cum_len == out_len;
    }
    bool CipherAES256GCMPacket::encrypt_alloc(
        const uint8_t* in, const uint32_t in_len,
        uint8_t*& out, uint32_t& out_len,
        const uint8_t* aad, const uint32_t aad_len
    ) {
        out = new uint8_t[out_len = enc_size(in_len)];
        return encrypt(in, in_len, out, out_len, aad, aad_len);
    }

    bool CipherAES256GCMPacket::decrypt(
        const uint8_t* in, const uint32_t in_len,
        uint8_t* out, const uint32_t out_len,
        const uint8_t* aad, const uint32_t aad_len
    ) {
        assert(out_len == dec_size(in_len));

        if (!EVP_DecryptInit_ex(_ctx, _algo, nullptr, nullptr, nullptr))
            return false;

        if (!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            return false;

        if (!EVP_DecryptInit_ex(_ctx, nullptr, nullptr, _key, _iv.bytes))
            return false;

        int len = 0;
        int cum_len = 0;

        if (aad != nullptr && aad_len == 0) {
            if (!EVP_DecryptUpdate(_ctx, nullptr, &len, aad, aad_len))
                return false;
        }

        //Data starts at 16 offset, auth tag before
        if (!EVP_DecryptUpdate(_ctx, out, &len, in + 16, in_len - 16))
            return false;
        cum_len += len;
        assert(cum_len <= out_len);

        //Set expected auth tag
        if (!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)in))
            return false;

        int ret = EVP_DecryptFinal_ex(_ctx, out + cum_len, &len);
        cum_len += len;
        assert(cum_len <= out_len);

        //Reset for next packet
        EVP_CIPHER_CTX_reset(_ctx);
        _iv.parts.packet++;

        //Successfully verified
        if (ret > 0) {
            return out_len == cum_len;
        }
        return false;
    }
    bool CipherAES256GCMPacket::decrypt_alloc(
        const uint8_t* in, const uint32_t in_len,
        uint8_t*& out, uint32_t& out_len,
        const uint8_t* aad, const uint32_t aad_len
    ) {
        out = new uint8_t[out_len = dec_size(in_len)];
        return decrypt(in, in_len, out, out_len, aad, aad_len);
    }

    CipherAES256GCMPacket::~CipherAES256GCMPacket() {
        if (_algo != nullptr) {
            EVP_CIPHER_free(_algo);
        }

        if (_ctx != nullptr) {
            EVP_CIPHER_CTX_free(_ctx);
        }
    }

}

