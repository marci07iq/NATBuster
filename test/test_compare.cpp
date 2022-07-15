#include <string>

#include "../common/crypto/pkey.h"

#include <openssl/err.h>

int main() {
    {
        using NATBuster::Common::Crypto::PKey;
        using NATBuster::Common::Utils::Blob;

        //Generate two different ED keys

        PKey key_a;
        if (!key_a.generate_ed25519()) goto error;

        PKey key_b;
        if (!key_b.generate_ed25519()) goto error;

        Blob a_public = Blob();
        Blob b_public = Blob();
      
        if (!key_a.export_public(a_public)) goto error;
        if (!key_b.export_public(b_public)) goto error;
        
        if (a_public.size() == 0) goto error;
        if (b_public.size() == 0) goto error;
        
        //Load the public key from A
        PKey key_a_pub;
        if (!key_a_pub.load_public(a_public)) goto error;

        //Load the public key (twice) from B
        PKey key_b_pub;
        if (!key_b_pub.load_public(b_public)) goto error;
        PKey key_b2_pub;
        if (!key_b2_pub.load_public(b_public)) goto error;

        //Compare the 5 previous keys in every possible order.

        if (!key_a.is_same(key_a)) goto error;
        if (!key_a.is_same(key_a_pub)) goto error;
        if (key_a.is_same(key_b)) goto error;
        if (key_a.is_same(key_b_pub)) goto error;
        if (key_a.is_same(key_b2_pub)) goto error;

        if (!key_a_pub.is_same(key_a)) goto error;
        if (!key_a_pub.is_same(key_a_pub)) goto error;
        if (key_a_pub.is_same(key_b)) goto error;
        if (key_a_pub.is_same(key_b_pub)) goto error;
        if (key_a_pub.is_same(key_b2_pub)) goto error;

        if (key_b.is_same(key_a)) goto error;
        if (key_b.is_same(key_a_pub)) goto error;
        if (!key_b.is_same(key_b)) goto error;
        if (!key_b.is_same(key_b_pub)) goto error;
        if (!key_b.is_same(key_b2_pub)) goto error;

        if (key_b_pub.is_same(key_a)) goto error;
        if (key_b_pub.is_same(key_a_pub)) goto error;
        if (!key_b_pub.is_same(key_b)) goto error;
        if (!key_b_pub.is_same(key_b_pub)) goto error;
        if (!key_b_pub.is_same(key_b2_pub)) goto error;

        if (key_b2_pub.is_same(key_a)) goto error;
        if (key_b2_pub.is_same(key_a_pub)) goto error;
        if (!key_b2_pub.is_same(key_b)) goto error;
        if (!key_b2_pub.is_same(key_b_pub)) goto error;
        if (!key_b2_pub.is_same(key_b2_pub)) goto error;

        return 0;
    }

error:
    printf("\n\n============ ERROR =============\n");
    printf("%s", ERR_error_string(ERR_get_error(), nullptr));

    return 1;
}