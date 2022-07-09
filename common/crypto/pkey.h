#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>

#include "../utils/copy_protection.h"
#include "../utils/blob.h"

namespace NATBuster::Common::Crypto {
    enum class PKeyAlgo : int {
        Ec25519 = NID_X25519,
        Ed25519 = NID_ED25519
    };

    class PKey : Utils::NonCopyable {
        EVP_PKEY* _key = nullptr;

        static int key_password_cb(char* buf, int size, int rwflag, void* u);

    public:
        PKey() {

        }

        PKey(PKey&& other) noexcept;

        PKey& operator=(PKey&& other) noexcept;

        inline bool has_key() const {
            return _key != nullptr;
        }
        inline void erase() {
            if (_key != nullptr) {
                EVP_PKEY_free(_key);
                _key = nullptr;
            }

        }

        bool generate(PKeyAlgo nid);
        bool generate_ec25519();
        bool generate_ed25519();

        bool load_file_private(std::string filename);
        bool load_public(const Utils::ConstBlobView& in);

        bool save_file_private(std::string filename) const;
        bool save_file_public(std::string filename) const;
        bool export_public(Utils::BlobView& out) const;

        //Ed25519 key only
        bool sign(const Utils::ConstBlobView& data_in, Utils::BlobView& sig_out) const;
        //Ed25519 key only
        bool verify(const Utils::ConstBlobView& data_in, const Utils::ConstBlobView& sig_in) const;

        //Ec25519 key only
        bool ecdhe(PKey& key_other, Utils::BlobView& secret_out) const;

        bool is_same(PKey& rhs) const {
            if (!has_key()) return false;
            if (!rhs.has_key()) return false;
            return 1 == EVP_PKEY_eq(_key, rhs._key);
        }
    };
};