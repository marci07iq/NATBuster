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

    PKey::PKey(PKey&& other) noexcept {
        _key = other._key;
        other._key = nullptr;
    }

    PKey& PKey::operator=(PKey&& other) noexcept {
        if(_key != nullptr) EVP_PKEY_free(_key);
        _key = other._key;
        other._key = nullptr;

        return *this;
    }

    bool PKey::generate(PKeyAlgo nid) {
        //Override protection
        if (_key != nullptr) return false;

        //Parameter generation ctx
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id((int)nid, nullptr);
        if (nullptr == ctx) {
            return false;
        }

        if (1 != EVP_PKEY_keygen_init(ctx)) {
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        if (1 != EVP_PKEY_keygen(ctx, &_key)) {
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        EVP_PKEY_CTX_free(ctx);

        return true;

    }

    bool PKey::generate_ec25519() {
        return  generate(PKeyAlgo::Ec25519);
    }

    bool PKey::generate_ed25519() {
        return generate(PKeyAlgo::Ed25519);
    }

    bool PKey::load_file_private(std::string filename) {
        //Dont overwrite
        if (_key != nullptr) return false;

        //Read file
        FILE* fp = fopen(filename.c_str(), "r");
        if (fp == nullptr) return false;

        //Read key
        _key = PEM_read_PrivateKey(fp, nullptr, key_password_cb, nullptr);

        //Close file
        fclose(fp);

        return (_key != nullptr);
    }

    bool PKey::load_public(const Utils::ConstBlobView& in) {
        //Dont overwrite
        if (_key != nullptr) return false;

        //Create BIO
        BIO* mem = BIO_new(BIO_s_mem());
        BIO_write(mem, in.getr(), in.size());

        //Read key
        _key = PEM_read_bio_PUBKEY(mem, nullptr, key_password_cb, nullptr);

        //Free BIO
        BIO_free(mem);

        return (_key != nullptr);
    }

    bool PKey::save_file_private(std::string filename) const {
        //Need to have a key
        if (_key == nullptr) return false;

        //Open file
        FILE* fp = fopen(filename.c_str(), "w");
        if (fp == nullptr) return false;

        //Write key
        //TODO: Add password support
        int res = PEM_write_PKCS8PrivateKey(fp, _key, nullptr, nullptr, 0, nullptr, nullptr);

        //Close file
        fclose(fp);

        return res == 1;
    }

    bool PKey::save_file_public(std::string filename) const {
        //Need to have a key
        if (_key == nullptr) return false;

        //Open file
        FILE* fp = fopen(filename.c_str(), "w");
        if (fp == nullptr) return false;

        //Write key
        int res = PEM_write_PUBKEY(fp, _key);

        //Close file
        fclose(fp);

        return res == 1;
    }

    bool PKey::export_public(Utils::BlobView& out) const {
        //Need to have a key
        if (_key == nullptr) return false;

        //Create BIO buffer
        BIO* mem = BIO_new(BIO_s_mem());

        //Write key
        int res = PEM_write_bio_PUBKEY(mem, _key);

        //Create output space
        out.resize(BIO_ctrl_pending(mem));

        //Copy out
        BIO_read(mem, out.getw(), out.size());
        assert(BIO_eof(mem));

        //Free mem
        BIO_free(mem);

        return res == 1;
    }

    bool PKey::sign(const Utils::ConstBlobView& data_in, Utils::BlobView& sig_out) const {
        //TODO: Error check;
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (1 != EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, _key)) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        size_t siglen;

        if (1 != EVP_DigestSign(md_ctx, nullptr, &siglen, data_in.getr(), data_in.size())) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        sig_out.resize(siglen);

        if (1 != EVP_DigestSign(md_ctx, sig_out.getw(), &siglen, data_in.getr(), data_in.size())) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        sig_out.resize(siglen);

        EVP_MD_CTX_free(md_ctx);

        return true;
    }

    bool PKey::verify(const Utils::ConstBlobView& data_in, const Utils::ConstBlobView& sig_in) const {
        //TODO: Error check;
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

        if (1 != EVP_DigestVerifyInit(md_ctx, nullptr, nullptr, nullptr, _key)) {
            EVP_MD_CTX_free(md_ctx);
            return false;
        }

        int res = EVP_DigestVerify(md_ctx, sig_in.getr(), sig_in.size(), data_in.getr(), data_in.size());

        EVP_MD_CTX_free(md_ctx);

        return res == 1;
    }

    bool PKey::ecdhe(PKey& key_other, Utils::BlobView& secret_out) const {
        EVP_PKEY_CTX* ctx;
        if (nullptr == (ctx = EVP_PKEY_CTX_new(_key, nullptr))) return false;

        if (1 != EVP_PKEY_derive_init(ctx)) {
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        if (1 != EVP_PKEY_derive_set_peer(ctx, key_other._key)) {
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        size_t secret_len_szt;
        if (1 != EVP_PKEY_derive(ctx, nullptr, &secret_len_szt)) {
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        secret_out.resize(secret_len_szt);
        
        if (1 != (EVP_PKEY_derive(ctx, secret_out.getw(), &secret_len_szt))) {
            EVP_PKEY_CTX_free(ctx);
            return false;
        }

        secret_out.resize(secret_len_szt);

        EVP_PKEY_CTX_free(ctx);
        return true;
    }
};