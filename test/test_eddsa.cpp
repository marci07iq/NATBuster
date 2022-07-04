#include <cstring>

#include "../common/crypto/pkey.h"

#include <openssl/err.h>

int main() {
    {
        //Signing needs Ed25519
        NATBuster::Common::Crypto::PKey key_a;
        if (!key_a.generate_ed25519()) goto error;

        uint8_t* a_public = nullptr;
        
        uint32_t a_public_len;
        
        if (!key_a.export_public(a_public, a_public_len)) goto error;
        
        if (a_public_len == 0) goto error;
       
        if (a_public == nullptr) goto error;
        
        NATBuster::Common::Crypto::PKey key_a_pub;
        if (!key_a_pub.load_public(a_public, a_public_len)) goto error;

        uint8_t* a_aign = nullptr;

        uint32_t a_sign_len;

        const char* data = "EhnfJjHhTRVJ6%(HVE32FgJI(NKGDF+DHJVBFrRECHu(gUZgUfZTvZTfd/%DZTVZD65it";

        if (!key_a.sign((const uint8_t*)data, 64, a_aign, a_sign_len)) goto error;

        if (a_aign == nullptr) goto error;
        if (a_sign_len == 0) goto error;

        if (!key_a_pub.verify((const uint8_t*)data, 64, a_aign, a_sign_len)) goto error;

        const char* hex = "0123456789ABCDEF";

        for (int i = 0; i < a_sign_len; i++) {
            std::cout << hex[a_aign[i] >> 4] << hex[a_aign[i] & 0xf] << ":";
        }
        std::cout << std::endl;
        
        return 0;
    }

error:
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}