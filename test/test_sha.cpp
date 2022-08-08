#include <cstring>

#include "../common/crypto/hash.h"
#include "../common/utils/hex.h"

#include <openssl/err.h>

int main() {
    {
        //Cant do ecdh on Ed25519
        NATBuster::Crypto::Hash sha256(NATBuster::Crypto::HashAlgo::SHA256);
        
        NATBuster::Utils::Blob data = NATBuster::Utils::Blob::factory_string("test");
        NATBuster::Utils::Blob result = NATBuster::Utils::Blob();

        if (!sha256.calc(data, result)) goto error;

        NATBuster::Utils::print_hex(result);
        std::cout << std::endl;

        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}