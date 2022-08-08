#include <cstring>

#include "../common/crypto/pkey.h"
#include "../common/utils/hex.h"

#include <openssl/err.h>

int main() {
    {
        //Signing needs Ed25519
        NATBuster::Crypto::PrKey key_a;
        if (!key_a.generate_ed25519()) goto error;

        NATBuster::Utils::Blob a_public = NATBuster::Utils::Blob();
        
        if (!key_a.export_public(a_public)) goto error;
        
        if (a_public.size() == 0) goto error;
       
        NATBuster::Crypto::PuKey key_a_pub;
        if (!key_a_pub.load_public(a_public)) goto error;

        NATBuster::Utils::Blob a_sign = NATBuster::Utils::Blob();

        NATBuster::Utils::Blob data = NATBuster::Utils::Blob::factory_string("Some test data to sign");

        if (!key_a.sign(data, a_sign)) goto error;

        if (a_sign.size() == 0) goto error;

        if (!key_a_pub.verify(data, a_sign)) goto error;

        NATBuster::Utils::print_hex(a_sign);
        std::cout << std::endl;

        NATBuster::Utils::Blob data2 = NATBuster::Utils::Blob::factory_string("Some fake data to sign");

        if (key_a_pub.verify(data2, a_sign)) goto error;

        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}