#include <cstring>

#include "../common/crypto/pkey.h"
#include "../common/utils/hex.h"

#include <openssl/err.h>

int main() {
    {
        //Cant do ecdh on Ed25519
        NATBuster::Crypto::PrKey key_a;
        if (!key_a.generate_ec25519()) goto error;

        NATBuster::Crypto::PrKey key_b;
        if (!key_b.generate_ec25519()) goto error;

        NATBuster::Utils::Blob a_public = NATBuster::Utils::Blob();

        NATBuster::Utils::Blob b_public = NATBuster::Utils::Blob();

        if (!key_a.export_public(a_public)) goto error;
        if (!key_b.export_public(b_public)) goto error;

        if (a_public.size() == 0) goto error;
        if (b_public.size() == 0) goto error;

        NATBuster::Crypto::PuKey key_a_pub;
        if (!key_a_pub.load_public(a_public)) goto error;

        NATBuster::Crypto::PuKey key_b_pub;
        if (!key_b_pub.load_public(b_public)) goto error;

        NATBuster::Utils::Blob ab_secret = NATBuster::Utils::Blob();

        NATBuster::Utils::Blob ba_secret = NATBuster::Utils::Blob();

        if (!key_a.ecdhe(key_b_pub, ab_secret)) goto error;
        if (!key_b.ecdhe(key_a_pub, ba_secret)) goto error;

        if (ab_secret.size() == 0) goto error;
        if (ba_secret.size() == 0) goto error;

        if (ab_secret.size() != ba_secret.size()) goto error;

        if (ab_secret.size() == 0) goto error;

        if (memcmp(ab_secret.getr(), ba_secret.getr(), ab_secret.size()) != 0) goto error;

        NATBuster::Utils::print_hex(ab_secret);

        std::cout << std::endl;
        
        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}