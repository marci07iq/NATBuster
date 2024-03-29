#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>

#include "hash.h"
#include "../utils/copy_protection.h"
#include "../utils/blob.h"

namespace NATBuster::Crypto {
    enum class PKeyAlgo : int {
        Undef = NID_undef,
        Ec25519 = NID_X25519,
        Ed25519 = NID_ED25519
    };

    class PKey : Utils::NonCopyable {
    protected:
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

        PKeyAlgo type() {
            int nid = EVP_PKEY_get_base_id(_key);
            switch (nid) {
            case EVP_PKEY_X25519:
                return PKeyAlgo::Ec25519;
            case EVP_PKEY_ED25519:
                return PKeyAlgo::Ed25519;
            }
            return PKeyAlgo::Undef;
        }

        bool is_same(const PKey& rhs) const {
            if (!has_key()) return false;
            if (!rhs.has_key()) return false;
            return 1 == EVP_PKEY_eq(_key, rhs._key);
        }

        virtual ~PKey();
    };

    class PuKey : public PKey {
        friend class PrKey;
    public:
        PuKey();
        PuKey(PuKey&& other) noexcept;
        PuKey& operator=(PuKey&& other) noexcept;

        
        bool load_file_public(std::string filename);
        bool load_public(const Utils::ConstBlobView& in);

        bool export_file_public(std::string filename) const;
        bool export_public(Utils::BlobView& out) const;

        bool copy_public_from(const PuKey& src);
        PuKey copy_public() const;

        //Ed25519 key only
        bool verify(const Utils::ConstBlobView& data_in, const Utils::ConstBlobView& sig_in) const;

        bool fingerprint(Utils::BlobView& out) const;
    };

    class PrKey : public PuKey {
        //Hide these functions from a private key
        using PuKey::load_file_public;
        using PuKey::load_public;
        using PuKey::copy_public_from;
    public:
        PrKey();

        PrKey(PrKey&& other) noexcept;

        PrKey& operator=(PrKey&& other) noexcept;

        bool generate(PKeyAlgo nid);
        bool generate_ec25519();
        bool generate_ed25519();

        bool load_file_private(std::string filename);
        bool load_private(const Utils::ConstBlobView& in);

        bool export_file_private(std::string filename) const;
        bool export_private(Utils::BlobView& out) const;

        bool copy_private_from(const PrKey& src);
        PrKey copy_private() const;

        //Ed25519 key only
        bool sign(const Utils::ConstBlobView& data_in, Utils::BlobView& sig_out) const;

        //Ec25519 key only
        bool ecdhe(PuKey& key_other, Utils::BlobView& secret_out) const;
    };
};