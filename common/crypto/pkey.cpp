#include "pkey.h"

namespace NATBuster::Common::Crypto {
    int PKey::key_password_cb(char* buf, int size, int rwflag, void* u)
    {
        std::string pass;
        if (rwflag == 0) {
            std::cout << "Enter pass phrase for key" << std::endl;

            std::cin >> pass;
        }
        else {
            if (rwflag == 0) {
                int i = 0;
                for (i = 0; i < 5; i++) {
                    std::cout << "Enter pass phrase for the new key" << std::endl;
                    std::cin >> pass;

                    std::string pass2;
                    std::cout << "Re-type:" << std::endl;
                    std::cin >> pass2;

                    if (pass == pass2) {
                        break;
                    }
                    else {
                        std::cout << "Try again" << std::endl;
                    }
                }
                if (i >= 5) return -1;
            }
        }

        int len = pass.size();
        len = (size < len) ? size : len;

        memcpy(buf, pass.c_str(), len);

        return len;
    }

    bool PKey::loaded() {
        return _key != NULL;
    }

    bool PKey::generate(PKeyAlgo nid) {
        //Override protection
        if (_key != NULL) return false;

        //Parameter generation ctx
        EVP_PKEY_CTX* param_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
        if (NULL == param_ctx) {
            return false;
        }

        if (1 != EVP_PKEY_paramgen_init(param_ctx)) {
            EVP_PKEY_CTX_free(param_ctx);
            return false;
        }

        if (1 != EVP_PKEY_CTX_set_ec_paramgen_curve_nid(param_ctx, (int)nid)) {
            EVP_PKEY_CTX_free(param_ctx);
            return false;
        }


        EVP_PKEY* params = NULL;
        if (!EVP_PKEY_paramgen(param_ctx, &params)) {
            EVP_PKEY_CTX_free(param_ctx);
            return false;
        }

        EVP_PKEY_CTX* key_ctx = EVP_PKEY_CTX_new(params, NULL);
        if (NULL == key_ctx) {
            EVP_PKEY_free(params);
            EVP_PKEY_CTX_free(param_ctx);
            return false;
        }


        if (1 != EVP_PKEY_keygen_init(key_ctx)) {
            EVP_PKEY_CTX_free(key_ctx);
            EVP_PKEY_free(params);
            EVP_PKEY_CTX_free(param_ctx);
            return false;
        }
        if (1 != EVP_PKEY_keygen(key_ctx, &_key)) {
            EVP_PKEY_CTX_free(key_ctx);
            EVP_PKEY_free(params);
            EVP_PKEY_CTX_free(param_ctx);
            return false;
        }

        EVP_PKEY_CTX_free(key_ctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(param_ctx);

        return true;

    }

    bool PKey::generate_ec25519() {
        return  generate(PKeyAlgo::Ec25519);
    }

    bool PKey::generate_ed25519() {
        return  generate(PKeyAlgo::Ed25519);
    }

    bool PKey::load_file_private(std::string filename) {
        //Dont overwrite
        if (_key != NULL) return false;

        //Read file
        FILE* fp = fopen(filename.c_str(), "r");
        if (fp == NULL) return false;

        //Read key
        _key = PEM_read_PrivateKey(fp, NULL, key_password_cb, NULL);

        //Close file
        fclose(fp);

        return (_key != NULL);
    }

    bool PKey::load_public(const uint8_t* out, uint32_t out_len) {
        //Dont overwrite
        if (_key != NULL) return false;

        //Create BIO
        BIO* mem = BIO_new(BIO_s_mem());
        BIO_write(mem, out, out_len);

        //Read key
        _key = PEM_read_bio_PUBKEY(mem, NULL, key_password_cb, NULL);

        //Free BIO
        BIO_free(mem);

        return (_key != NULL);
    }

    bool PKey::save_file_private(std::string filename) {
        //Need to have a key
        if (_key == NULL) return false;

        //Open file
        FILE* fp = fopen(filename.c_str(), "w");
        if (fp == NULL) return false;

        //Write key
        //TODO: Add password support
        int res = PEM_write_PKCS8PrivateKey(fp, _key, NULL, NULL, 0, NULL, NULL);

        //Close file
        fclose(fp);

        return res == 1;
    }

    bool PKey::save_file_public(std::string filename) {
        //Need to have a key
        if (_key == NULL) return false;

        //Open file
        FILE* fp = fopen(filename.c_str(), "w");
        if (fp == NULL) return false;

        //Write key
        int res = PEM_write_PUBKEY(fp, _key);

        //Close file
        fclose(fp);

        return res == 1;
    }

    bool PKey::export_public(uint8_t*& out, uint32_t& out_len) {
        //Need to have a key
        if (_key == NULL) return false;

        //Create BIO buffer
        BIO* mem = BIO_new(BIO_s_mem());

        //Write key
        int res = PEM_write_bio_PUBKEY(mem, _key);

        //Create output space
        out_len = BIO_ctrl_pending(mem);
        out = new uint8_t[out_len];

        //Copy out
        BIO_read(mem, out, out_len);
        assert(BIO_eof(mem));

        //Free mem
        BIO_free(mem);

        return res == 1;
    }

    bool PKey::sign(const uint8_t* data, const uint32_t data_len, uint8_t*& sig_out, uint32_t& sig_out_len) {
        //TODO: Error check;
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (1 != EVP_DigestSignInit(md_ctx, NULL, NULL, NULL, _key)) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        size_t siglen;

        if (1 != EVP_DigestSign(md_ctx, NULL, &siglen, data, data_len)) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        sig_out = new uint8_t[siglen];

        if (1 != EVP_DigestSign(md_ctx, sig_out, &siglen, data, data_len)) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        EVP_MD_CTX_free(md_ctx);

        sig_out_len = siglen;

        return true;
    }

    bool PKey::verify(const uint8_t* data, const uint32_t data_len, const uint8_t* sig, const uint32_t sig_len) {
        //TODO: Error check;
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (1 != EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, _key)) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        size_t siglen;

        int res = EVP_DigestVerify(md_ctx, sig, sig_len, data, data_len);

        EVP_MD_CTX_free(md_ctx);

        return res == 1;
    }
};