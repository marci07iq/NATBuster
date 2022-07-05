#include <cstring>

#include "../common/crypto/hash.h"
#include "../common/utils/hex.h"

#include <openssl/err.h>

int main() {
    {
        //Cant do ecdh on Ed25519
        NATBuster::Common::Crypto::Hash sha256(NATBuster::Common::Crypto::HashAlgo::SHA256);
        
        NATBuster::Common::Utils::Blob data = NATBuster::Common::Utils::Blob::factory_string("test");
        NATBuster::Common::Utils::Blob result = NATBuster::Common::Utils::Blob();

        NATBuster::Common::Utils::BlobView data_view = NATBuster::Common::Utils::BlobView(&data);
        NATBuster::Common::Utils::BlobView result_view = NATBuster::Common::Utils::BlobView(&result);

        if (!sha256.calc(data_view, result_view)) goto error;

        NATBuster::Common::Utils::print_hex(result_view);
        std::cout << std::endl;

        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}