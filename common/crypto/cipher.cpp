#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "cipher.h"

namespace NATBuster::Crypto {

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

    CipherPacketStream::CipherPacketStream(EVP_CIPHER_CTX* ctx, EVP_CIPHER* algo) :
        _ctx(ctx),
        _algo(algo) {

    }

    CipherPacketStream::CipherPacketStream(CipherPacketStream&& other) noexcept {
        _ctx = other._ctx;
        other._ctx = nullptr;

        _algo = other._algo;
        other._algo = nullptr;

    }
    CipherPacketStream& CipherPacketStream::operator=(CipherPacketStream&& other) noexcept {
        if (_ctx != nullptr) EVP_CIPHER_CTX_free(_ctx);
        _ctx = other._ctx;
        other._ctx = nullptr;

        if (_algo != nullptr) EVP_CIPHER_free(_algo);
        _algo = other._algo;
        other._algo = nullptr;

        return *this;
    }

    CipherPacketStream::~CipherPacketStream() {
        if (_algo != nullptr) {
            EVP_CIPHER_free(_algo);
        }

        if (_ctx != nullptr) {
            EVP_CIPHER_CTX_free(_ctx);
        }
    }

    //AES-GCM

    CipherAES256GCMPacketStream::CipherAES256GCMPacketStream() : CipherPacketStream(
        EVP_CIPHER_CTX_new(),
        EVP_CIPHER_fetch(nullptr, "AES-256-GCM", nullptr)
    ) {
        assert(4 <= _tag_size - _tag_standalone_size);
        assert(_tag_size - _tag_standalone_size <= 8);
    }

    CipherAES256GCMPacketStream::CipherAES256GCMPacketStream(CipherAES256GCMPacketStream&& other) noexcept :
        CipherPacketStream(std::move(other)) {
        
        for (int i = 0; i < 32; i++) {
            _key[i] = other._key[i];
            other._key[i] = 0;
        }

        _iv.parts.common = other._iv.parts.common;
        other._iv.parts.common = 0;

        _iv.parts.packet = other._iv.parts.packet;
        other._iv.parts.packet = 0;
    }

    CipherAES256GCMPacketStream& CipherAES256GCMPacketStream::operator=(CipherAES256GCMPacketStream&& other) noexcept {
        CipherPacketStream::operator=(std::move(other));


        for (int i = 0; i < 32; i++) {
            _key[i] = other._key[i];
            other._key[i] = 0;
        }

        _iv.parts.common = other._iv.parts.common;
        other._iv.parts.common = 0;

        _iv.parts.packet = other._iv.parts.packet;
        other._iv.parts.packet = 0;

        return *this;
    }

    uint32_t CipherAES256GCMPacketStream::iv_size() {
        return 12;
    }
    void CipherAES256GCMPacketStream::set_iv(const uint8_t* bytes, uint32_t size) {
        if (size != 12) {
            throw 1;
        }
        for (uint32_t i = 0; i < 12 && i < size; i++) {
            _iv.bytes[i] = bytes[i];
        }
    }
    void CipherAES256GCMPacketStream::set_iv_common(uint32_t common) {
        _iv.parts.common = common;
    }
    void CipherAES256GCMPacketStream::set_iv_packet_normal(uint64_t packet) {
        _sequential_packet_counter = packet;
    }
    void CipherAES256GCMPacketStream::set_iv_packet_standalone(uint64_t packet) {
        _standalone_packet_counter = packet;
    }

    uint32_t CipherAES256GCMPacketStream::key_size() {
        return 32;
    }
    void CipherAES256GCMPacketStream::set_key(const uint8_t* key, uint32_t size) {
        if (size != 32) {
            throw 1;
        }
        for (int i = 0; i < 32; i++) {
            _key[i] = key[i];
        }
    }

    uint32_t CipherAES256GCMPacketStream::max_size_increase() {
        return _tag_size;
    }

    uint32_t CipherAES256GCMPacketStream::enc_size(uint32_t raw_size) {
        return raw_size + _tag_size;
    }
    uint32_t CipherAES256GCMPacketStream::dec_size(uint32_t enc_size) {
        return enc_size - _tag_size;
    }

    bool CipherAES256GCMPacketStream::encrypt(
        const Utils::ConstBlobView& in,
        Utils::BlobView& out,
        const Utils::ConstBlobView& aad
    ) {
        _iv.parts.packet = _sequential_packet_counter;

        if (1 != EVP_EncryptInit_ex(_ctx, _algo, nullptr, nullptr, nullptr))
            return false;

        if (1 != EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            return false;

        if (1 != EVP_EncryptInit_ex(_ctx, nullptr, nullptr, _key, _iv.bytes))
            return false;

        int len;
        //Add Additional Authentication Data if needed
        if (aad.size() != 0) {
            if (1 != EVP_EncryptUpdate(_ctx, nullptr, &len, aad.getr(), aad.size()))
                return false;
        }

        //Allocate output space
        out.resize(in.size() + 16);
        //View for the checksum
        Utils::BlobSliceView checksum = out.slice(0, _tag_size);
        //View for the cipher text
        Utils::BlobSliceView cipher = out.slice(_tag_size, in.size());

        //Encrypt data
        if (1 != EVP_EncryptUpdate(_ctx, cipher.getw(), &len, in.getr(), in.size()))
            return false;

        if (len < 0) {
            assert(false);
            return false;
        }
        uint32_t total_len = len;
        assert(total_len <= cipher.size());

        Utils::BlobSliceView cipher_remain = cipher.slice_right(total_len);

        //Encrypt data (finalise)
        //For GCM, this doesnt actually produce any more bytes.
        //For block aligned, it would add padding
        if (1 != EVP_EncryptFinal_ex(_ctx, cipher_remain.getw(), &len))
            return false;

        total_len += len;
        assert(total_len == cipher.size());

        //Get auth tag, to the beginning of out
        if (1 != EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_GET_TAG, _tag_size, checksum.getw()))
            return false;

        //Reset for next packet
        EVP_CIPHER_CTX_reset(_ctx);
        ++_sequential_packet_counter;

        return true;
    }

    bool CipherAES256GCMPacketStream::decrypt(
        const Utils::ConstBlobView& in,
        Utils::BlobView& out,
        const Utils::ConstBlobView& aad
    ) {
        _iv.parts.packet = _sequential_packet_counter;

        if (in.size() < _tag_size) return false;

        if (!EVP_DecryptInit_ex(_ctx, _algo, nullptr, nullptr, nullptr))
            return false;

        if (!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            return false;

        if (!EVP_DecryptInit_ex(_ctx, nullptr, nullptr, _key, _iv.bytes))
            return false;

        //View for the checksum
        const Utils::ConstBlobView& checksum = in.cslice_left(_tag_size);
        //View for the cipher text
        const Utils::ConstBlobView& cipher = in.cslice_right(_tag_size);
        //Allocate output space
        out.resize(cipher.size());


        int len;
        if (aad.size() != 0) {
            if (!EVP_DecryptUpdate(_ctx, nullptr, &len, aad.getr(), aad.size()))
                return false;
        }

        //Data starts at 16 offset, auth tag before
        if (!EVP_DecryptUpdate(_ctx, out.getw(), &len, cipher.getr(), cipher.size()))
            return false;
        if (len < 0) {
            assert(false);
            return false;
        }
        uint32_t total_len = len;
        assert(total_len <= out.size());

        //Set expected auth tag
        if (!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_TAG, _tag_size, (void*)checksum.getr()))
            return false;

        Utils::BlobSliceView out_remain = out.slice_right(total_len);

        int ret = EVP_DecryptFinal_ex(_ctx, out_remain.getw(), &len);
        total_len += len;
        assert(total_len == out.size());

        //Reset for next packet
        EVP_CIPHER_CTX_reset(_ctx);
        ++_sequential_packet_counter;

        //Successfully verified
        if (ret > 0) {
            return true;
        }
        return false;
    }

    bool CipherAES256GCMPacketStream::encrypt_standalone(
        const Utils::ConstBlobView& in,
        Utils::BlobView& out,
        const Utils::ConstBlobView& aad
    ) {
        _iv.parts.packet = _standalone_packet_counter;

        if (1 != EVP_EncryptInit_ex(_ctx, _algo, nullptr, nullptr, nullptr))
            return false;

        if (1 != EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            return false;

        if (1 != EVP_EncryptInit_ex(_ctx, nullptr, nullptr, _key, _iv.bytes))
            return false;

        int len;
        //Add Additional Authentication Data if needed
        if (aad.size() != 0) {
            if (1 != EVP_EncryptUpdate(_ctx, nullptr, &len, aad.getr(), aad.size()))
                return false;
        }

        //Allocate output space
        out.resize(in.size() + _tag_size);
        //View for the checksum
        Utils::BlobSliceView checksum = out.slice(0, _tag_standalone_size);
        //View for the counter
        Utils::BlobSliceView counter = out.slice(_tag_standalone_size, _tag_size - _tag_standalone_size);
        //View for the cipher text
        Utils::BlobSliceView cipher = out.slice(_tag_size, in.size());

        //Encrypt data
        if (1 != EVP_EncryptUpdate(_ctx, cipher.getw(), &len, in.getr(), in.size()))
            return false;

        if (len < 0) {
            assert(false);
            return false;
        }
        uint32_t total_len = len;
        assert(total_len <= cipher.size());

        Utils::BlobSliceView cipher_remain = cipher.slice_right(total_len);

        //Encrypt data (finalise)
        //For GCM, this doesnt actually produce any more bytes.
        //For block aligned, it would add padding
        if (1 != EVP_EncryptFinal_ex(_ctx, cipher_remain.getw(), &len))
            return false;

        total_len += len;
        assert(total_len == cipher.size());

        //Get auth tag, to the beginning of out
        if (1 != EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_GET_TAG, _tag_standalone_size, checksum.getw()))
            return false;

        //Write the counter
        Utils::Blob counter_num = Utils::Blob::factory_copy((uint8_t*)&_iv.parts.packet, _tag_size - _tag_standalone_size);
        counter.copy_from(counter_num);

        //Reset for next packet
        EVP_CIPHER_CTX_reset(_ctx);
        ++_standalone_packet_counter;

        return true;
    }

    bool CipherAES256GCMPacketStream::decrypt_standalone(
        const Utils::ConstBlobView& in,
        Utils::BlobView& out,
        const Utils::ConstBlobView& aad
    ) {
        if (in.size() < 16) return false;

        //Packet counter
        const Utils::ConstBlobView& counter = in.cslice(_tag_standalone_size, _tag_size - _tag_standalone_size);
        _iv.parts.packet = 0;
        memcpy(&_iv.parts.packet, counter.getr(), counter.size());

        if (!EVP_DecryptInit_ex(_ctx, _algo, nullptr, nullptr, nullptr))
            return false;

        if (!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr))
            return false;

        if (!EVP_DecryptInit_ex(_ctx, nullptr, nullptr, _key, _iv.bytes))
            return false;

        //View for the checksum
        const Utils::ConstBlobView& checksum = in.cslice_left(_tag_standalone_size);
        //View for the cipher text
        const Utils::ConstBlobView& cipher = in.cslice_right(_tag_size);
        //Allocate output space
        out.resize(cipher.size());


        int len;
        if (aad.size() != 0) {
            if (!EVP_DecryptUpdate(_ctx, nullptr, &len, aad.getr(), aad.size()))
                return false;
        }

        //Data starts at 16 offset, auth tag before
        if (!EVP_DecryptUpdate(_ctx, out.getw(), &len, cipher.getr(), cipher.size()))
            return false;
        if (len < 0) {
            assert(false);
            return false;
        }
        uint32_t total_len = len;
        assert(total_len <= out.size());

        //Set expected auth tag
        if (!EVP_CIPHER_CTX_ctrl(_ctx, EVP_CTRL_GCM_SET_TAG, _tag_standalone_size, (void*)checksum.getr()))
            return false;

        Utils::BlobSliceView out_remain = out.slice_right(total_len);

        int ret = EVP_DecryptFinal_ex(_ctx, out_remain.getw(), &len);
        total_len += len;
        assert(total_len == out.size());

        //Reset for next packet
        EVP_CIPHER_CTX_reset(_ctx);

        //Successfully verified
        if (ret > 0) {
            return true;
        }
        return false;
    }

    CipherAES256GCMPacketStream::~CipherAES256GCMPacketStream() {
        
    }

}

