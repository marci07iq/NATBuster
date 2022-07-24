#include <openssl/ec.h>
#include <openssl/err.h>

#include "../common/crypto/pkey.h"
#include "../common/utils/hex.h"

using NATBuster::Common::Utils::Blob;
using NATBuster::Common::Utils::print_hex;
using NATBuster::Common::Crypto::PKey;
using NATBuster::Common::Crypto::Hash;
using NATBuster::Common::Crypto::HashAlgo;

int main() {
    {
        PKey key;

        if (!key.generate_ed25519()) goto error;

        Blob fingerprint;
        if (!key.fingerprint(fingerprint)) goto error;

        print_hex(fingerprint);

        return 0;
    }
error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;

}