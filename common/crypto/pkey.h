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

        PKey(PKey&& other);

        PKey& operator=(PKey&& other);

        bool loaded();

        bool generate(PKeyAlgo nid);

        bool generate_ec25519();

        bool generate_ed25519();

        bool load_file_private(std::string filename);

        bool load_public(const Utils::ConstBlobView& in);

        bool save_file_private(std::string filename);

        bool save_file_public(std::string filename);

        bool export_public(Utils::BlobView& out);

        //Ed25519 key only
        bool sign(const Utils::ConstBlobView& data_in, Utils::BlobView& sig_out);

        //Ed25519 key only
        bool verify(const Utils::ConstBlobView& data_in, const Utils::ConstBlobView& sig_in);

        //Ec25519 key only
        bool ecdhe(PKey& key_other, Utils::BlobView& secret_out);
    };
};