#include <cstring>

#include "../common/crypto/pkey.h"
#include "../common/utils/hex.h"

#include <openssl/err.h>

int main() {
    {
        //Signing needs Ed25519
        NATBuster::Common::Crypto::PKey key_a;
        if (!key_a.generate_ed25519()) goto error;

        NATBuster::Common::Utils::Blob a_public = NATBuster::Common::Utils::Blob();
        NATBuster::Common::Utils::BlobView a_public_view = NATBuster::Common::Utils::BlobView(&a_public);
        
        if (!key_a.export_public(a_public_view)) goto error;
        
        if (a_public_view.size() == 0) goto error;
       
        NATBuster::Common::Crypto::PKey key_a_pub;
        if (!key_a_pub.load_public(a_public_view)) goto error;

        NATBuster::Common::Utils::Blob a_sign = NATBuster::Common::Utils::Blob();
        NATBuster::Common::Utils::BlobView a_sign_view = NATBuster::Common::Utils::BlobView(&a_sign);

        NATBuster::Common::Utils::Blob data = NATBuster::Common::Utils::Blob::factory_string("Some test data to sign");
        NATBuster::Common::Utils::BlobView data_view = NATBuster::Common::Utils::BlobView(&data);

        if (!key_a.sign(data_view, a_sign_view)) goto error;

        if (a_sign_view.size() == 0) goto error;

        if (!key_a_pub.verify(data_view, a_sign_view)) goto error;

        NATBuster::Common::Utils::print_hex(a_sign_view);
        std::cout << std::endl;

        NATBuster::Common::Utils::Blob data2 = NATBuster::Common::Utils::Blob::factory_string("Some fake data to sign");
        NATBuster::Common::Utils::BlobView data2_view = NATBuster::Common::Utils::BlobView(&data2);

        if (key_a_pub.verify(data2_view, a_sign_view)) goto error;

        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}