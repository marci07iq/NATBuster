#include <iostream>

#include <openssl/evp.h>

unsigned char* ecdh(size_t* secret_len)
{
    EVP_PKEY_CTX* pctx, *kctx;
    EVP_PKEY_CTX* ctx;
    unsigned char* secret;
    EVP_PKEY* pkey = NULL, * peerkey, * params = NULL;
    /* NB: assumes pkey, peerkey have been already set up */

    /* Create the context for parameter generation */
    if (NULL == (pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL))) throw 1;

    /* Initialise the parameter generation */
    if (1 != EVP_PKEY_paramgen_init(pctx)) throw 1;

    /* We're going to use the ANSI X9.62 Prime 256v1 curve */
    if (1 != EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1)) throw 1;

    /* Create the parameter object params */
    if (!EVP_PKEY_paramgen(pctx, &params)) throw 1;

    /* Create the context for the key generation */
    if (NULL == (kctx = EVP_PKEY_CTX_new(params, NULL))) throw 1;

    /* Generate the key */
    if (1 != EVP_PKEY_keygen_init(kctx)) throw 1;
    if (1 != EVP_PKEY_keygen(kctx, &pkey)) throw 1;

    /* Get the peer's public key, and provide the peer with our public key -
     * how this is done will be specific to your circumstances */
    peerkey = get_peerkey(pkey);

    /* Create the context for the shared secret derivation */
    if (NULL == (ctx = EVP_PKEY_CTX_new(pkey, NULL))) throw 1;

    /* Initialise */
    if (1 != EVP_PKEY_derive_init(ctx)) throw 1;

    /* Provide the peer public key */
    if (1 != EVP_PKEY_derive_set_peer(ctx, peerkey)) throw 1;

    /* Determine buffer length for shared secret */
    if (1 != EVP_PKEY_derive(ctx, NULL, secret_len)) throw 1;

    /* Create the buffer */
    if (NULL == (secret = OPENSSL_malloc(*secret_len))) throw 1;

    /* Derive the shared secret */
    if (1 != (EVP_PKEY_derive(ctx, secret, secret_len))) throw 1;

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(peerkey);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(kctx);
    EVP_PKEY_free(params);
    EVP_PKEY_CTX_free(pctx);

    /* Never use a derived secret directly. Typically it is passed
     * through some hash function to produce a key */
    return secret;
}



int main() {
    //Dont expect to find anything fun here. It's keymashing. Or is it?
    const unsigned char* plaintext = (const unsigned char*)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const unsigned char* key = (const unsigned char*)"Fh%n+|i!=En-A%r_=oi!48*%6ASN/rs$";
    const unsigned char* aad = (const unsigned char*)nullptr;
    const unsigned char* iv = (const unsigned char*)"H7%-~bD)Dj*|f_k-N[F]k/!*h Fl!/}P";

    unsigned char* encrypted = new unsigned char[100];
    unsigned char* tag = new unsigned char[100];
    unsigned char* decrypted = new unsigned char[100];

    int en_len = gcm_encrypt(
        plaintext, 51,
        aad, 0,
        key,
        iv, 12,
        encrypted, tag);

    std::cout << en_len << std::endl;

    int dec_len = gcm_decrypt(
        encrypted, en_len,
        aad, 0,
        tag, key,
        iv, 12,
        decrypted
    );

    std::cout << dec_len << std::endl;

    for (int i = 0; i < en_len + 1; i++) {
        std::cout << decrypted[i];
    }

}