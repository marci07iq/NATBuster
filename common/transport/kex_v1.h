#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include "../utils/copy_protection.h"
#include "../utils/blob.h"

#include "../crypto/pkey.h"
#include "../crypto/hash.h"
#include "../crypto/cipher.h"
#include "../crypto/random.h"

#include "kex.h"

namespace NATBuster::Common::Proto {
    //Protocol V1.0.00:

    // A->B: PDHa, Na
    // B: K = PDHa * SDSb, H = hash(K, PDHa, PDHb, Na, Nb)
    // A<-B: PDHb, Nb, PLTb, H_SLTb
    // A: Calculate K, Check H signature
    // A<-B: New keys
    // A: All future packets will be received via a key derived from K
    // A->B: New keys
    // B: ALl future packets will be received with a key derived from K
    // A->B: PLTa, (H, M1, M2, PLTa)_SLTa

    class KEXV1 : public KEX {
    protected:
        //States for the 
        enum State : uint8_t {
            //KEX object created, no messages sent/received yet
            S0_New = 0,
            //Previous key exchange complete, return to idle
            SF_Done = 16,

            //Protocol error has occured. Irrecoverable state.
            SF_Err = 17,

            //Client hello sent/received A->B
            S1_CHello = 1,
            //Server hello sent/received B->A
            S2_SHello = 2,
            //Server encryption newkeys sent/received B->A
            S3_SEnc = 3,
            //Client encryption newkeys sent/received B->A
            S4_CEnc = 4,
            //Client identity sent/received B->A
            S5_CIdentity = 5,
        } _state;

        enum PacketType : uint8_t {
            //Client hello (C->S)
            //Client DH-public key, client nonce
            //Allowed in state 
            KEXC_HELLO = 0,
            //Server hello (S->C)
            //Server DH-public key, server LT-public key, 
            KEXS_HELLO = 1,
            //Server new keys (S->C)
            //Server is ready to switch to new keys
            //Will send all subsequent messages with new keys
            KEXS_NEWKEYS = 2,
            //Client accepts KEXS_HELLO 
            //Client begins sending with new keys after this message
            KEXC_NEWKEYS = 3,
            //Client sends identity
            KEXC_IDENTITY = 4

        } type;

        //The initial client capabilities, and server capabilities message
        //To auth agains downgrade attacks
        Utils::Blob _m1, _m2;

        //The DH shared secret
        Utils::Blob _k;
        //THe server auth shared hash
        Utils::Blob _h;

        //Nonces used during the challenge/response

        Utils::Blob _nonce_a;
        Utils::Blob _nonce_b;

        //Ephemeral DH keys used for the kex

        Crypto::PKey _dh_key_a;
        Crypto::PKey _dh_key_b;

        //Long term private key of this side
        //Long term public key of other side
        //If empty, any remote is accepted

        Crypto::PKey _lt_key_a;
        Crypto::PKey _lt_key_b;

        KEXV1(
            Crypto::PKey&& lt_key_a, Crypto::PKey&& lt_key_b,
            Utils::Blob&& m1, Utils::Blob&& m2) :
            _lt_key_a(std::move(lt_key_a)),
            _lt_key_b(std::move(lt_key_b)),
            _m1(std::move(m1)),
            _m2(std::move(m2)) {

        }

        //Wipe the internal state
        inline KEX::KEX_Event fail(KEX::KEX_Event reason) {
            _state = SF_Err;
            return reason;
        }

        inline void kex_reset() {
            _nonce_a.erase();
            _nonce_b.erase();
            _k.erase();
            _h.erase();

            _dh_key_a.erase();
            _dh_key_b.erase();
        }

        bool secret_hash_calculate() {
            Utils::Blob hash_content;
            Utils::PackedBlobWriter hash_content_w(hash_content);

            //Hash is of (K, PDHa, PDHb, Na, Nb)

            hash_content_w.add_record(_k);

            Utils::Blob dh_key_a;
            if (!_dh_key_a.export_public(dh_key_a)) return false;
            hash_content_w.add_record(dh_key_a);

            Utils::Blob dh_key_b;
            if (!_dh_key_a.export_public(dh_key_b)) return false;
            hash_content_w.add_record(dh_key_b);

            hash_content_w.add_record(_nonce_a);
            hash_content_w.add_record(_nonce_b);

            Crypto::Hash hasher(Crypto::HashAlgo::SHA512);
            if (!hasher.calc(hash_content, _h)) return false;

            return true;
        }

        bool client_proof_hash(Utils::BlobView& dst) const {
            Utils::Blob hash_content;
            Utils::PackedBlobWriter hash_content_w(hash_content);

            //Hash is of (H, M1, M2, PLTa)

            hash_content_w.add_record(_h);

            hash_content_w.add_record(_m1);

            hash_content_w.add_record(_m2);

            Utils::Blob lt_key_a;
            if (!_lt_key_a.export_public(lt_key_a)) return false;
            hash_content_w.add_record(lt_key_a);

            Crypto::Hash hasher(Crypto::HashAlgo::SHA512);
            if (!hasher.calc(hash_content, dst)) return false;

            return true;
        }

        //A: A->B key
        //B: B->A key
        //C: A->B iv
        //D: B->A iv
        //E: A->B aad
        //F: B->A aad
        inline bool secret_hash_derive_bits(Utils::BlobView& out, char c, uint32_t min_len) {
            out.resize(0);
            uint32_t frag_cnt = 0;
            while (out.size() < min_len) {
                Utils::Blob c_blob = Utils::Blob::factory_empty(5);
                *((uint32_t*)c_blob.getw()) = frag_cnt;
                frag_cnt++;
                c_blob.getw()[4] = (uint8_t)c;

                Utils::Blob hash_data = Utils::Blob::factory_concat(
                    { &_k, &c_blob, &_h }
                );

                Crypto::Hash hash(Crypto::HashAlgo::SHA512);
                if (!hash.calc(hash_data, out)) return false;
            }
            return true;
        }
    };

    //KEX for the initiator side
    class KEXV1_A : public KEXV1 {
        //Client side states:
        //S0_New / SF_Done
        //->KEXC_HELLO
        //S1_CHello
        // 
        //<-KEXS_HELLO
        //S2_SHello
        // 
        //<-KEXS_NEWKEYS
        //S3_SEnc
        //->KEXC_NEWKEYS
        //S4_CEnc
        //->KEXC_IDENTITY
        //S5_CIdentity
        //SF_Done
    public:
        KEXV1_A(
            Crypto::PKey&& my_private, Crypto::PKey&& other_public,
            Utils::Blob&& m1, Utils::Blob&& m2);


        KEX::KEX_Event recv(const Utils::ConstBlobView& packet, Transport::EncryptOPT* out);

        KEX::KEX_Event init_kex(Transport::EncryptOPT* out);
    };

    //KEX for the target side
    class KEXV1_B : public KEXV1 {
        //Server side states:
        //S0_New / SF_Done
        // 
        //<-KEXC_HELLO
        //S1_CHello
        //->KEXS_HELLO
        //S2_SHello
        //->KEXS_NEWKEYS
        //S3_SEnc
        // 
        //<-KEXC_NEWKEYS
        //S4_CEnc
        // 
        //<-KEXC_IDENTITY
        //S5_CIdentity
        //SF_Done
    public:
        KEXV1_B(
            Crypto::PKey&& my_private, Crypto::PKey&& other_public,
            Utils::Blob&& m1, Utils::Blob&& m2);

        KEX::KEX_Event recv(const Utils::ConstBlobView& packet, Transport::EncryptOPT* out);

        KEX::KEX_Event init_kex(Transport::EncryptOPT* out);
    };
}