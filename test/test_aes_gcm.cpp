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

        NATBuster::Common::Crypto::CipherAES256GCMPacketStream encrypt;
        NATBuster::Common::Crypto::CipherAES256GCMPacketStream decrypt;

        encrypt.set_iv_common(iv_common_0);
        decrypt.set_iv_common(iv_common_0);

        encrypt.set_iv_packet(iv_packet_0);
        decrypt.set_iv_packet(iv_packet_0);

        {
            const NATBuster::Common::Utils::Blob data = NATBuster::Common::Utils::Blob::factory_string("Fg=/F76UztD/fU64rcUZgI/Tö9oHHZvUTRd+Ug8=HKJZViztF)UVgjGOU;FUTZgkhBU");
            NATBuster::Common::Utils::Blob cipher = NATBuster::Common::Utils::Blob();
            NATBuster::Common::Utils::Blob aad = NATBuster::Common::Utils::Blob();

            if (!encrypt.encrypt(data, cipher, aad)) goto error;

            NATBuster::Common::Utils::Blob plaintext = NATBuster::Common::Utils::Blob();

            if (!decrypt.decrypt(cipher, plaintext, aad)) goto error;

            if (plaintext.size() != data.size()) goto error;
            if (0 != memcmp(data.getr(), plaintext.getr(), data.size())) goto error;
        }

        {
            const NATBuster::Common::Utils::Blob data = NATBuster::Common::Utils::Blob::factory_string("=/IuzbVUZVZTr7Gztc=(%R987Gut(%HgLKHOI=t/RTVIU//%Tv%UTiZGfOUZvKHTCiz5fjVVJD");
            NATBuster::Common::Utils::Blob cipher = NATBuster::Common::Utils::Blob();
            NATBuster::Common::Utils::Blob aad = NATBuster::Common::Utils::Blob();

            if (!encrypt.encrypt(data, cipher, aad)) goto error;

            NATBuster::Common::Utils::Blob plaintext = NATBuster::Common::Utils::Blob();

            if (!decrypt.decrypt(cipher, plaintext, aad)) goto error;

            if (plaintext.size() != data.size()) goto error;
            if (0 != memcmp(data.getr(), plaintext.getr(), data.size())) goto error;
        }

        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}