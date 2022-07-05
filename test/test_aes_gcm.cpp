#include <cstring>

#include "../common/crypto/random.h"
#include "../common/crypto/cipher.h"

#include <openssl/err.h>

int main() {
    {
        uint32_t iv_common_0;

        if (!NATBuster::Common::Crypto::random_u32(iv_common_0)) goto error;

        uint64_t iv_packet_0;

        if (!NATBuster::Common::Crypto::random_u64(iv_packet_0)) goto error;

        NATBuster::Common::Crypto::CipherAES256GCMPacket encrypt;
        NATBuster::Common::Crypto::CipherAES256GCMPacket decrypt;

        encrypt.set_iv_common(iv_common_0);
        decrypt.set_iv_common(iv_common_0);

        encrypt.set_iv_packet(iv_packet_0);
        decrypt.set_iv_packet(iv_packet_0);

        const char* data = "Fg=/F76UztD/fU64rcUZgI/Tö9oHHZvUTRd+Ug8=HKJZViztF)UVgjGOU;FUTZgkhBU";
        const uint32_t data_len = 37;

        uint8_t* cipher = nullptr;
        uint32_t cipher_len = 0;

        if(!encrypt.encrypt_alloc((const uint8_t*)data, data_len, cipher, cipher_len, nullptr, 0)) goto error;

        uint8_t* plaintext = nullptr;
        uint32_t plaintext_len = 0;

        if(!decrypt.decrypt_alloc(cipher, cipher_len, plaintext, plaintext_len, nullptr, 0)) goto error;

        if (plaintext_len != data_len) goto error;

        if (0 != memcmp(data, plaintext, data_len)) goto error;

        delete[] plaintext;
        delete[] cipher;

        const char* data2 = "=/IuzbVUZVZTr7Gztc=(%R987Gut(%HgLKHOI=t/RTVIU//%Tv%UTiZGfOUZvKHTCiz5fjVVJD";
        const uint32_t data2_len = 37;

        if (!encrypt.encrypt_alloc((const uint8_t*)data2, data2_len, cipher, cipher_len, nullptr, 0)) goto error;

        if (!decrypt.decrypt_alloc(cipher, cipher_len, plaintext, plaintext_len, nullptr, 0)) goto error;

        if (0 != memcmp(data2, plaintext, data2_len)) goto error;

        delete[] plaintext;
        delete[] cipher;

        cipher = nullptr;
        plaintext = nullptr;

        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}