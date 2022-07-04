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

        bool load_public(const uint8_t* out, uint32_t out_len);

        bool save_file_private(std::string filename);

        bool save_file_public(std::string filename);

        bool export_public(uint8_t*& out, uint32_t& out_len);

        //Ed25519 key only
        bool sign(const uint8_t* data, const uint32_t data_len, uint8_t*& sig_out, uint32_t& sig_out_len);

        //Ed25519 key only
        bool verify(const uint8_t* data, const uint32_t data_len, const uint8_t* sig, const uint32_t sig_len);

        //Ec25519 key only
        bool ecdhe(PKey& key_other, uint8_t*& secret, uint32_t& secret_len);
    };
};