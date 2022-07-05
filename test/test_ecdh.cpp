#include <cstring>

#include "../common/crypto/pkey.h"
#include "../common/utils/hex.h"

#include <openssl/err.h>

int main() {
    {
        //Cant do ecdh on Ed25519
        NATBuster::Common::Crypto::PKey key_a;
        if (!key_a.generate_ec25519()) goto error;

        NATBuster::Common::Crypto::PKey key_b;
        if (!key_b.generate_ec25519()) goto error;

        NATBuster::Common::Utils::Blob a_public = NATBuster::Common::Utils::Blob();
        NATBuster::Common::Utils::BlobView a_public_view = NATBuster::Common::Utils::BlobView(&a_public);

        NATBuster::Common::Utils::Blob b_public = NATBuster::Common::Utils::Blob();
        NATBuster::Common::Utils::BlobView b_public_view = NATBuster::Common::Utils::BlobView(&b_public);

        if (!key_a.export_public(a_public_view)) goto error;
        if (!key_b.export_public(b_public_view)) goto error;

        if (a_public_view.size() == 0) goto error;
        if (b_public_view.size() == 0) goto error;

        NATBuster::Common::Crypto::PKey key_a_pub;
        if (!key_a_pub.load_public(a_public_view)) goto error;

        NATBuster::Common::Crypto::PKey key_b_pub;
        if (!key_b_pub.load_public(b_public_view)) goto error;

        NATBuster::Common::Utils::Blob ab_secret = NATBuster::Common::Utils::Blob();
        NATBuster::Common::Utils::BlobView ab_secret_view = NATBuster::Common::Utils::BlobView(&ab_secret);

        NATBuster::Common::Utils::Blob ba_secret = NATBuster::Common::Utils::Blob();
        NATBuster::Common::Utils::BlobView ba_secret_view = NATBuster::Common::Utils::BlobView(&ba_secret);

        if (!key_a.ecdhe(key_b_pub, ab_secret_view)) goto error;
        if (!key_b.ecdhe(key_a_pub, ba_secret_view)) goto error;

        if (ab_secret_view.size() == 0) goto error;
        if (ba_secret_view.size() == 0) goto error;

        if (ab_secret_view.size() != ba_secret_view.size()) goto error;

        if (ab_secret_view.size() == 0) goto error;

        if (memcmp(ab_secret_view.get(), ba_secret_view.get(), ab_secret_view.size()) != 0) goto error;

        NATBuster::Common::Utils::print_hex(ab_secret_view);

        std::cout << std::endl;
        
        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}