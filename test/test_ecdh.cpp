#include <cstring>

#include "../common/crypto/pkey.h"

#include <openssl/err.h>

int main() {
    {
        //Cant do ecdh on Ed25519
        NATBuster::Common::Crypto::PKey key_a;
        if (!key_a.generate_ec25519()) goto error;

        NATBuster::Common::Crypto::PKey key_b;
        if (!key_b.generate_ec25519()) goto error;

        uint8_t* a_public = nullptr;
        uint8_t* b_public = nullptr;

        uint32_t a_public_len;
        uint32_t b_public_len;

        if (!key_a.export_public(a_public, a_public_len)) goto error;
        if (!key_b.export_public(b_public, b_public_len)) goto error;

        if (a_public_len == 0) goto error;
        if (b_public_len == 0) goto error;

        if (a_public == nullptr) goto error;
        if (b_public == nullptr) goto error;

        NATBuster::Common::Crypto::PKey key_a_pub;
        if (!key_a_pub.load_public(a_public, a_public_len)) goto error;

        NATBuster::Common::Crypto::PKey key_b_pub;
        if (!key_b_pub.load_public(b_public, b_public_len)) goto error;

        uint8_t* ab_secret = nullptr;
        uint8_t* ba_secret = nullptr;

        uint32_t ab_secret_len;
        uint32_t ba_secret_len;

        if (!key_a.ecdhe(key_b_pub, ab_secret, ab_secret_len)) goto error;
        if (!key_b.ecdhe(key_a_pub, ba_secret, ba_secret_len)) goto error;

        if (ab_secret == nullptr) goto error;
        if (ba_secret == nullptr) goto error;

        if (ab_secret_len != ba_secret_len) goto error;

        if (ab_secret_len == 0) goto error;

        if (memcmp(ab_secret, ba_secret, ab_secret_len) != 0) goto error;

        const char* hex = "0123456789ABCDEF";

        for (int i = 0; i < ab_secret_len; i++) {
            std::cout << hex[ab_secret[i] >> 4] << hex[ab_secret[i] & 0xf] << ":";
        }
        std::cout << std::endl;
        
        return 0;
    }

error:
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}