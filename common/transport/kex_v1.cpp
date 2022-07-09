#include "kex_v1.h"
#include "encrypt.h"

namespace NATBuster::Common::Proto {
    KEXV1_A::KEXV1_A(
        Crypto::PKey&& my_private, Crypto::PKey&& other_public,
        Utils::Blob&& m1, Utils::Blob&& m2) :
        KEXV1(
            std::move(my_private), std::move(other_public),
            std::move(m1), std::move(m2)
        ) {
    }


    KEX::KEX_Event KEXV1_A::recv(const Utils::ConstBlobView& packet, Transport::EncryptOPT* out) {
        if (packet.size() == 0) return fail(KEX_Event::ErrorMalformed);

        PacketType packet_type = (PacketType)(*((uint8_t*)packet.getr()));
        const Utils::ConstBlobSliceView packet_data = packet.cslice_right(1);
        Utils::PackedBlobReader packet_reader(packet_data);

        switch (_state)
        {
            //Client Hello sent, expecting server hello as response
        case NATBuster::Common::Proto::KEXV1::S1_CHello:
        {
            if (packet_type != PacketType::KEXS_HELLO) return fail(KEX_Event::ErrorState);

            //Parse server hello

            //Remote DH key
            Utils::ConstBlobSliceView dh_key_b;
            if (!packet_reader.next_record(dh_key_b)) return fail(KEX_Event::ErrorMalformed);
            if (!_dh_key_b.load_public(dh_key_b)) return fail(KEX_Event::ErrorCrypto);
            if (!_dh_key_a.ecdhe(_dh_key_b, _k)) return fail(KEX_Event::ErrorCrypto);

            //Remote nonce
            Utils::ConstBlobSliceView nonce_b;
            if (!packet_reader.next_record(nonce_b)) return fail(KEX_Event::ErrorMalformed);
            _nonce_b.erase();
            _nonce_b.copy_from(nonce_b);

            //Calcualte secret hash
            if (!secret_hash_calculate()) return fail(KEX_Event::ErrorCrypto);

            //Remote public key
            Utils::ConstBlobSliceView lt_key_b;
            if (!packet_reader.next_record(lt_key_b)) return fail(KEX_Event::ErrorMalformed);
            Crypto::PKey lt_key_b_obj;
            if (!lt_key_b_obj.load_public(lt_key_b)) return fail(KEX_Event::ErrorCrypto);
            //Check if there is a trusted key, and compare
            if (_lt_key_b.has_key()) {
                if (!_lt_key_b.is_same(lt_key_b_obj)) return fail(KEX_Event::ErrorNotrust);
            }
            else {
                std::cout << "WARN: Implicit trusting remote key" << std::endl;
                _lt_key_b = std::move(lt_key_b_obj);
            }

            //Message integrity signature
            Utils::ConstBlobSliceView h_sign;
            if (!packet_reader.next_record(h_sign)) return fail(KEX_Event::ErrorMalformed);

            if (!_lt_key_b.verify(_h, h_sign)) return fail(KEX_Event::ErrorNotrust);

            //There should be nothing left in the inbound data
            if (!packet_reader.eof()) return fail(KEX_Event::ErrorMalformed);

            _state = S2_SHello;
            return KEX_Event::OK;
        }
        break;
        //Server Hello received, should get a Server New keys immediately after
        case NATBuster::Common::Proto::KEXV1::S2_SHello:
        {
            if (packet_type != PacketType::KEXS_NEWKEYS) return fail(KEX_Event::ErrorState);

            if (packet_data.size() != 0) return fail(KEX_Event::ErrorMalformed);

            Utils::Blob crypto_data;
            uint32_t crypto_size;

            //Set inbound keys
            crypto_size = out->_inbound.key_size();
            if (!secret_hash_derive_bits(crypto_data, 'A', crypto_size)) return fail(KEX_Event::ErrorCrypto);
            assert(crypto_size <= crypto_data.size());
            out->_inbound.set_key(crypto_data.getr(), crypto_size);

            crypto_size = out->_inbound.iv_size();
            if (!secret_hash_derive_bits(crypto_data, 'C', crypto_size)) return fail(KEX_Event::ErrorCrypto);
            assert(crypto_size <= crypto_data.size());
            out->_inbound.set_iv(crypto_data.getr(), crypto_size);

            _state = NATBuster::Common::Proto::KEXV1::S3_SEnc;




            //Send client new key packet
            {
                Utils::Blob c_newkey_packet = Utils::Blob::factory_empty(1, 0, 0);

                *((uint8_t*)c_newkey_packet.getw()) = (uint8_t)PacketType::KEXC_NEWKEYS;

                out->send(c_newkey_packet);
            }

            //Set outbound keys
            crypto_size = out->_outbound.key_size();
            if (!secret_hash_derive_bits(crypto_data, 'B', crypto_size)) return fail(KEX_Event::ErrorCrypto);
            assert(crypto_size <= crypto_data.size());
            out->_outbound.set_key(crypto_data.getr(), crypto_size);

            crypto_size = out->_outbound.iv_size();
            if (!secret_hash_derive_bits(crypto_data, 'D', crypto_size)) return fail(KEX_Event::ErrorCrypto);
            assert(crypto_size <= crypto_data.size());
            out->_outbound.set_iv(crypto_data.getr(), crypto_size);

            _state = NATBuster::Common::Proto::KEXV1::S4_CEnc;

            //Send client identity
            {
                //Blob for storing the data
                Utils::Blob c_id = Utils::Blob::factory_empty(1, 0, 200);

                //Set packet type
                *((uint8_t*)c_id.getw()) = (uint8_t)PacketType::KEXC_HELLO;

                //Writer for packing the data
                Utils::PackedBlobWriter c_id_w(c_id);

                //Write public key
                Utils::BlobSliceView lt_key_b = c_id_w.prepare_writeable_record();
                if (!_dh_key_a.export_public(lt_key_b)) return fail(KEX_Event::ErrorCrypto);
                c_id_w.finish_writeable_record(lt_key_b);

                //Write identity proof
                Utils::BlobSliceView client_proof = c_id_w.prepare_writeable_record();
                if (!client_proof_hash(client_proof))  return fail(KEX_Event::ErrorCrypto);
                c_id_w.finish_writeable_record(client_proof);
            }

            _state = NATBuster::Common::Proto::KEXV1::S5_CIdentity;
            _state = NATBuster::Common::Proto::KEXV1::SF_Done;

            return KEX_Event::OK_Done;
        }
        break;

        //No kex related packet form the server should be received in any of these states
        case NATBuster::Common::Proto::KEXV1::S0_New:
        case NATBuster::Common::Proto::KEXV1::S3_SEnc:
        case NATBuster::Common::Proto::KEXV1::S4_CEnc:
        case NATBuster::Common::Proto::KEXV1::S5_CIdentity:
        case NATBuster::Common::Proto::KEXV1::SF_Err:
        case NATBuster::Common::Proto::KEXV1::SF_Done:
        default:
            return fail(KEX_Event::ErrorState);
            break;
        }
    }

    KEX::KEX_Event KEXV1_A::init_kex(Transport::EncryptOPT* out) {
        //KEX can only be be called when a previos kex is done
        if (_state == S0_New || _state == SF_Done) {
            //Clean memory garbage
            kex_reset();
            //Create new DH key
            if (!_dh_key_a.generate_ec25519()) return fail(KEX_Event::ErrorCrypto);
            //Create new nonce
            if (!Crypto::random(_nonce_a, 64)) return fail(KEX_Event::ErrorCrypto);

            //Blob for storing the data
            Utils::Blob c_hello = Utils::Blob::factory_empty(1, 0, 200);

            //Set packet type
            *((uint8_t*)c_hello.getw()) = (uint8_t)PacketType::KEXC_HELLO;

            //Writer for packing the data
            Utils::PackedBlobWriter c_hello_w(c_hello);

            //Write nonce
            c_hello_w.add_record(_nonce_a);

            //Write dh public key
            Utils::BlobSliceView pkey = c_hello_w.prepare_writeable_record();
            _dh_key_a.export_public(pkey);
            c_hello_w.finish_writeable_record(pkey);

            out->send(c_hello);
            _state = S1_CHello;
            return KEX_Event::OK;
        }
        _state = SF_Err;
        return KEX_Event::ErrorState;
    }
}