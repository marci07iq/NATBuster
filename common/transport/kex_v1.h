#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include <openssl/evp.h>

#include "../utils/copy_protection.h"
#include "../utils/blob.h"

#include "../crypto/pkey.h"
#include "../crypto/hash.h"
#include "../crypto/cipher.h"

#include "kex.h"

namespace NATBuster::Common::Proto {
    //Protocol V1.0.00:

    // A->B: PDHa, Na
    // B: K = PDHa * SDSb, H = hash(K, PDHa, PDHb, Na, Nb)
    // A<-B: PDHb, Nb, H_SLTb
    // A: Calculate K, Check H signature
    // A<-B: New keys
    // A: All future packets will be received via a key derived from K
    // A->B: New keys
    // B: ALl future packets will be received with a key derived from K
    // A->B: PLTa, (H, M1, M2, PLTa)_SLTa

    class KEXV1 : public KEX {
    protected:
        enum PacketType : uint8_t {
            //Client hello (C->S)
            //Client DH-public key, client nonce
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

    private:
        //A: A->B key
        //B: B->A key
        //C: A->B iv
        //D: B->A iv
        //E: A->B aad
        //F: B->A aad
        inline void get_secret_hash(Utils::BlobView& out, char c, uint32_t min_len) {
            out.resize(0);
            while (out.size() < min_len) {
                Utils::Blob c_blob = Utils::Blob::factory_empty(5);
                *((uint32_t*)c_blob.getw()) = more;
                c_blob.getw()[4] = (uint8_t)c;

                Utils::Blob hash_data = Utils::Blob::factory_concat(
                    { &_k, &c_blob, &_h }
                );

                Crypto::Hash hash(Crypto::HashAlgo::SHA512);
                hash.calc(hash_data, out);
            }
        }

    public:
        //To be called after an OK_Newkey event
        virtual void set_key(Crypto::CipherPacketStream& stream) {
            Utils::Blob key_data;
            get_secret_hash(key_data, 'A');
            assert(stream.key_size() == 32);
            assert()
            stream.set_key(key_data.getr(), 32);
        }
    };

    //KEX for the initiator side
    class KEX_A {
        //States for the 
        enum State : uint8_t {
            //KEX object created, no messages sent yet
            S0_New = 0,
            //Initial message sent A->B
            //Client sends: 
            S1_
        } _state;

    };

    //KEX for the target side
    class KEX_B {

    };
}