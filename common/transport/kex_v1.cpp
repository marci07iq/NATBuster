#include "kex_v1.h"
#include "opt_session.h"

#include "../utils/version.h"

namespace NATBuster::Proto {
    KEXV1_A::KEXV1_A(
        const std::shared_ptr<const Crypto::PrKey> self,
        const std::shared_ptr<const Identity::UserGroup> known_remotes
    ) :
        KEXV1(self, known_remotes) {
        _state = State::S0_New;
    }


    KEX::KEX_Event KEXV1_A::recv(const Utils::ConstBlobView& packet, Transport::OPTSession* out) {
        if (packet.size() == 0) return fail(KEX_Event::ErrorMalformed);

        PacketType packet_type = (PacketType)(*((uint8_t*)packet.getr()));
        const Utils::ConstBlobSliceView packet_data = packet.cslice_right(1);
        Utils::PackedBlobReader packet_reader(packet_data);

        switch (_state)
        {
            //Send M1 (client version)
            //Should receive M2 (server version) next

        case NATBuster::Proto::KEXV1::S1_M1:
        {
            if (packet_type != PacketType::KEXS_VERSION) return fail(KEX_Event::ErrorState);
            if (packet_data.size() != 4) return fail(KEX_Event::ErrorMalformed);
            Utils::version version_v(*(uint32_t*)packet_data.getr());
            if(version_v.field.major != 1) return fail(KEX_Event::ErrorMalformed);
            //Clean memory garbage
            kex_reset();

            _m2.copy_from(packet, 0);

            _state = KEXV1::S2_M2;

            //Create new DH key
            if (!_dh_key_self.generate_ec25519()) return fail(KEX_Event::ErrorCrypto);
            //Create new nonce
            if (!Crypto::random(_nonce_a, 64)) return fail(KEX_Event::ErrorCrypto);

            //Blob for storing the data
            Utils::Blob c_hello = Utils::Blob::factory_empty(1, 0, 200);

            //Set packet type
            *((uint8_t*)c_hello.getw()) = (uint8_t)PacketType::KEXC_HELLO;

            //Writer for packing the data
            Utils::PackedBlobWriter c_hello_w(c_hello);

            //Write dh public key
            Utils::BlobSliceView pkey = c_hello_w.prepare_writeable_record();
            if (!_dh_key_self.export_public(pkey)) return fail(KEX_Event::ErrorCrypto);
            c_hello_w.finish_writeable_record(pkey);

            //Write nonce
            c_hello_w.add_record(_nonce_a);

            out->send_kex(c_hello);
            _state = K1_CHello;
            return KEX_Event::OK;
        }
        break;

        //Client Hello sent, expecting server hello as response
        case NATBuster::Proto::KEXV1::K1_CHello:
        {
            if (packet_type != PacketType::KEXS_HELLO) return fail(KEX_Event::ErrorState);

            //Parse server hello

            //Remote DH key
            Utils::ConstBlobSliceView dh_key_b;
            if (!packet_reader.next_record(dh_key_b)) return fail(KEX_Event::ErrorMalformed);
            if (!_dh_key_remote.load_public(dh_key_b)) return fail(KEX_Event::ErrorCrypto);
            if (!_dh_key_self.ecdhe(_dh_key_remote, _k)) return fail(KEX_Event::ErrorCrypto);

            //Remote nonce
            Utils::ConstBlobSliceView nonce_b;
            if (!packet_reader.next_record(nonce_b)) return fail(KEX_Event::ErrorMalformed);
            _nonce_b.erase();
            _nonce_b.copy_from(nonce_b);

            //Calcualte secret hash
            if (!secret_hash_calculate(true)) return fail(KEX_Event::ErrorCrypto);

            //Remote public key
            Utils::ConstBlobSliceView lt_key_b;
            if (!packet_reader.next_record(lt_key_b)) return fail(KEX_Event::ErrorMalformed);
            Crypto::PuKey lt_key_b_obj;
            if (!lt_key_b_obj.load_public(lt_key_b)) return fail(KEX_Event::ErrorCrypto);
            //Check if we already know the key
            if (_lt_key_remote.has_key()) {
                //The new key must match
                if(!_lt_key_remote.is_same(lt_key_b_obj))return fail(KEX_Event::ErrorNotrust);
            }
            //First time login
            else {
                //If a pool of allowed keys was set
                if (_users_remote) {
                    //New key must be member
                    if (!_users_remote->isMember(lt_key_b_obj)) return fail(KEX_Event::ErrorNotrust);
                    _lt_key_remote = std::move(lt_key_b_obj);
                    _user_remote = _users_remote->findUser(_lt_key_remote);
                }
                //If no allowed keys were set, implicitly trust anyone
                else {
                    std::cout << "WARN: Implicit trusting remote key" << std::endl;
                    _lt_key_remote = std::move(lt_key_b_obj);
                    _user_remote = Identity::User::Anonymous;
                }
                
            }

            //Message integrity signature
            Utils::ConstBlobSliceView h_sign;
            if (!packet_reader.next_record(h_sign)) return fail(KEX_Event::ErrorMalformed);

            if (!_lt_key_remote.verify(_h, h_sign)) return fail(KEX_Event::ErrorNotrust);

            //There should be nothing left in the inbound data
            if (!packet_reader.eof()) return fail(KEX_Event::ErrorMalformed);

            _state = K2_SHello;
            return KEX_Event::OK;
        }
        break;
        //Server Hello received, should get a Server New keys immediately after
        case NATBuster::Proto::KEXV1::K2_SHello:
        {
            if (packet_type != PacketType::KEXS_NEWKEYS) return fail(KEX_Event::ErrorState);

            if (!packet_reader.eof()) return fail(KEX_Event::ErrorMalformed);

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

            out->_flags._enc_on_ib = true;

            _state = NATBuster::Proto::KEXV1::K3_SEnc;




            //Send client new key packet
            {
                std::lock_guard _lg(out->_out_encryption_lock);

                Utils::Blob c_newkey_packet = Utils::Blob::factory_empty(1, 0, 0);

                *((uint8_t*)c_newkey_packet.getw()) = (uint8_t)PacketType::KEXC_NEWKEYS;

                out->send_kex(c_newkey_packet);

                out->_flags._enc_on_ob = true;
            
                //Set outbound keys
                crypto_size = out->_outbound.key_size();
                if (!secret_hash_derive_bits(crypto_data, 'B', crypto_size)) return fail(KEX_Event::ErrorCrypto);
                assert(crypto_size <= crypto_data.size());
                out->_outbound.set_key(crypto_data.getr(), crypto_size);

                crypto_size = out->_outbound.iv_size();
                if (!secret_hash_derive_bits(crypto_data, 'D', crypto_size)) return fail(KEX_Event::ErrorCrypto);
                assert(crypto_size <= crypto_data.size());
                out->_outbound.set_iv(crypto_data.getr(), crypto_size);

            }

            _state = NATBuster::Proto::KEXV1::K4_CEnc;

            //Send client identity
            {
                //Blob for storing the data
                Utils::Blob c_id = Utils::Blob::factory_empty(1, 0, 200);

                //Set packet type
                *((uint8_t*)c_id.getw()) = (uint8_t)PacketType::KEXC_IDENTITY;

                //Writer for packing the data
                Utils::PackedBlobWriter c_id_w(c_id);

                //Write public key
                Utils::BlobSliceView lt_key_self = c_id_w.prepare_writeable_record();
                if (!_lt_key_self->export_public(lt_key_self)) return fail(KEX_Event::ErrorCrypto);
                c_id_w.finish_writeable_record(lt_key_self);

                //Calculate proof data
                Utils::Blob client_proof_data = Utils::Blob::factory_empty(0, 0, 200);
                if (!client_proof_hash(client_proof_data, *_lt_key_self.get()))  return fail(KEX_Event::ErrorCrypto);

                //Write identity proof
                Utils::BlobSliceView client_proof = c_id_w.prepare_writeable_record();
                if (!_lt_key_self->sign(client_proof_data, client_proof)) return fail(KEX_Event::ErrorCrypto);
                c_id_w.finish_writeable_record(client_proof);

                out->send_kex(c_id);
            }

            _state = NATBuster::Proto::KEXV1::K5_CIdentity;
            _state = NATBuster::Proto::KEXV1::KF_Done;

            return KEX_Event::OK_Done;
        }
        break;

        //No kex related packet form the server should be received in any of these states
        case NATBuster::Proto::KEXV1::S0_New:
        case NATBuster::Proto::KEXV1::K3_SEnc:
        case NATBuster::Proto::KEXV1::K4_CEnc:
        case NATBuster::Proto::KEXV1::K5_CIdentity:
        case NATBuster::Proto::KEXV1::KF_Err:
        case NATBuster::Proto::KEXV1::KF_Done:
        default:
            return fail(KEX_Event::ErrorState);
            break;
        }
    }

    KEX::KEX_Event KEXV1_A::init_kex(Transport::OPTSession* out) {
        //KEX can only be be called when a previos kex is done
        if (_state == S0_New || _state == KF_Done) {
            _m1 = Utils::Blob::factory_empty(5, 0, 0);

            //Set packet type
            *((uint8_t*)_m1.getw()) = (uint8_t)PacketType::KEXC_VERSION;

            *((uint32_t*)(_m1.getw() + 1)) = Utils::current_version.num;

            out->send_kex(_m1);
            _state = KEXV1::S1_M1;
            return KEX_Event::OK;
        }
        _state = KF_Err;
        return KEX_Event::ErrorState;
    }


    KEXV1_B::KEXV1_B(
        const std::shared_ptr<const Crypto::PrKey> self,
        const std::shared_ptr<const Identity::UserGroup> known_remotes
    ) :
        KEXV1(self, known_remotes) {
        _state = State::S0_New;
    }


    KEX::KEX_Event KEXV1_B::recv(const Utils::ConstBlobView& packet, Transport::OPTSession* out) {
        if (packet.size() == 0) return fail(KEX_Event::ErrorMalformed);

        PacketType packet_type = (PacketType)(*((uint8_t*)packet.getr()));
        const Utils::ConstBlobSliceView packet_data = packet.cslice_right(1);
        Utils::PackedBlobReader packet_reader(packet_data);

        switch (_state)
        {

        case NATBuster::Proto::KEXV1::S0_New:
        {
            if (packet_type != PacketType::KEXC_VERSION) return fail(KEX_Event::ErrorState);
            if (packet_data.size() != 4) return fail(KEX_Event::ErrorMalformed);
            Utils::version version_v(*(uint32_t*)packet_data.getr());
            if (version_v.field.major != 1) return fail(KEX_Event::ErrorMalformed);

            _m1.copy_from(packet, 0);

            _m2 = Utils::Blob::factory_empty(5, 0, 0);

            //Set packet type
            *((uint8_t*)_m2.getw()) = (uint8_t)PacketType::KEXS_VERSION;

            *((uint32_t*)(_m2.getw() + 1)) = Utils::current_version.num;

            out->send_kex(_m2);
            _state = S2_M2;
            return KEX_Event::OK;
        }
        break;
            //Not curretnyl in a kex. Should start with a client hello
        case NATBuster::Proto::KEXV1::S2_M2:
        case NATBuster::Proto::KEXV1::KF_Done:
        {
            if (packet_type != PacketType::KEXC_HELLO) return fail(KEX_Event::ErrorState);

            //Clean memory garbage
            kex_reset();
            //Create new DH key
            if (!_dh_key_self.generate_ec25519()) return fail(KEX_Event::ErrorCrypto);
            //Create new nonce
            if (!Crypto::random(_nonce_b, 64)) return fail(KEX_Event::ErrorCrypto);

            //Parse client hello

            //Remote DH key
            Utils::ConstBlobSliceView dh_key_a;
            if (!packet_reader.next_record(dh_key_a)) return fail(KEX_Event::ErrorMalformed);
            if (!_dh_key_remote.load_public(dh_key_a)) return fail(KEX_Event::ErrorCrypto);
            if (!_dh_key_self.ecdhe(_dh_key_remote, _k)) return fail(KEX_Event::ErrorCrypto);

            //Remote nonce
            Utils::ConstBlobSliceView nonce_a;
            if (!packet_reader.next_record(nonce_a)) return fail(KEX_Event::ErrorMalformed);
            _nonce_a.erase();
            _nonce_a.copy_from(nonce_a);

            //Calcualte secret hash
            if (!secret_hash_calculate(false)) return fail(KEX_Event::ErrorCrypto);

            //Should not have any more inbound data
            if (!packet_reader.eof()) return fail(KEX_Event::ErrorMalformed);

            _state = State::K1_CHello;

            //Prepare server hello
            Utils::Blob s_hello = Utils::Blob::factory_empty(1, 0, 500);

            //Set packet type
            *((uint8_t*)s_hello.getw()) = (uint8_t)PacketType::KEXS_HELLO;

            //Writer for packing the data
            Utils::PackedBlobWriter s_hello_w(s_hello);

            //Write dh public key
            Utils::BlobSliceView ekey = s_hello_w.prepare_writeable_record();
            if (!_dh_key_self.export_public(ekey)) return fail(KEX_Event::ErrorCrypto);
            s_hello_w.finish_writeable_record(ekey);

            //Write nonce
            s_hello_w.add_record(_nonce_b);

            //Write lt public key
            Utils::BlobSliceView lkey = s_hello_w.prepare_writeable_record();
            if (!_lt_key_self->export_public(lkey)) return fail(KEX_Event::ErrorCrypto);
            s_hello_w.finish_writeable_record(lkey);

            //Write h signature
            Utils::BlobSliceView h_sign = s_hello_w.prepare_writeable_record();
            if (!_lt_key_self->sign(_h, h_sign)) return fail(KEX_Event::ErrorCrypto);
            s_hello_w.finish_writeable_record(h_sign);

            out->send_kex(s_hello);
            _state = State::K2_SHello;

            //Send server new key packet
            {
                std::lock_guard _lg(out->_out_encryption_lock);

                Utils::Blob s_newkey_packet = Utils::Blob::factory_empty(1, 0, 0);

                *((uint8_t*)s_newkey_packet.getw()) = (uint8_t)PacketType::KEXS_NEWKEYS;

                out->send_kex(s_newkey_packet);

                out->_flags._enc_on_ob = true;


                //Set outbound keys
                Utils::Blob crypto_data;
                uint32_t crypto_size;

                crypto_size = out->_outbound.key_size();
                if (!secret_hash_derive_bits(crypto_data, 'A', crypto_size)) return fail(KEX_Event::ErrorCrypto);
                assert(crypto_size <= crypto_data.size());
                out->_outbound.set_key(crypto_data.getr(), crypto_size);

                crypto_size = out->_outbound.iv_size();
                if (!secret_hash_derive_bits(crypto_data, 'C', crypto_size)) return fail(KEX_Event::ErrorCrypto);
                assert(crypto_size <= crypto_data.size());
                out->_outbound.set_iv(crypto_data.getr(), crypto_size);
            }

            _state = NATBuster::Proto::KEXV1::K3_SEnc;
            return KEX_Event::OK;
        }
        break;
        //Server new keys sent, should get client new keys back
        case NATBuster::Proto::KEXV1::K3_SEnc:
        {
            if (packet_type != PacketType::KEXC_NEWKEYS) return fail(KEX_Event::ErrorState);

            if (!packet_reader.eof()) return fail(KEX_Event::ErrorMalformed);

            //Set inbound keys
            Utils::Blob crypto_data;
            uint32_t crypto_size;

            crypto_size = out->_inbound.key_size();
            if (!secret_hash_derive_bits(crypto_data, 'B', crypto_size)) return fail(KEX_Event::ErrorCrypto);
            assert(crypto_size <= crypto_data.size());
            out->_inbound.set_key(crypto_data.getr(), crypto_size);

            crypto_size = out->_inbound.iv_size();
            if (!secret_hash_derive_bits(crypto_data, 'D', crypto_size)) return fail(KEX_Event::ErrorCrypto);
            assert(crypto_size <= crypto_data.size());
            out->_inbound.set_iv(crypto_data.getr(), crypto_size);

            out->_flags._enc_on_ib = true;

            _state = NATBuster::Proto::KEXV1::K4_CEnc;
            return KEX_Event::OK;
        }
        break;
        //Client new keys received, should get client identity
        case NATBuster::Proto::KEXV1::K4_CEnc:
        {
            if (packet_type != PacketType::KEXC_IDENTITY) return fail(KEX_Event::ErrorState);

            //Remote LT key
            Utils::ConstBlobSliceView lt_key_a;
            if (!packet_reader.next_record(lt_key_a)) return fail(KEX_Event::ErrorMalformed);
            Crypto::PuKey lt_key_a_obj;
            if (!lt_key_a_obj.load_public(lt_key_a)) return fail(KEX_Event::ErrorCrypto);
            //Check if we already know the key
            if (_lt_key_remote.has_key()) {
                //The new key must match
                if (!_lt_key_remote.is_same(lt_key_a_obj))return fail(KEX_Event::ErrorNotrust);
            }
            //First time login
            else {
                //If a pool of allowed keys was set
                if (_users_remote) {
                    //New key must be member
                    if (!_users_remote->isMember(lt_key_a_obj)) return fail(KEX_Event::ErrorNotrust);

                    _lt_key_remote = std::move(lt_key_a_obj);
                    _user_remote = _users_remote->findUser(_lt_key_remote);
                }
                //If no allowed keys were set, implicitly trust anyone
                else {
                    std::cout << "WARN: Implicit trusting remote key" << std::endl;

                    _lt_key_remote = std::move(lt_key_a_obj);
                    _user_remote = Identity::User::Anonymous;
                }
                
            }

            //Signature
            Utils::ConstBlobSliceView client_proof_signature;
            if (!packet_reader.next_record(client_proof_signature)) return fail(KEX_Event::ErrorMalformed);

            if (!packet_reader.eof()) return fail(KEX_Event::ErrorMalformed);
            _state = K5_CIdentity;

            //Calculate proof data
            Utils::Blob client_proof_data = Utils::Blob::factory_empty(0, 0, 200);
            if (!client_proof_hash(client_proof_data, _lt_key_remote))  return fail(KEX_Event::ErrorCrypto);

            //Check signature
            if (!_lt_key_remote.verify(client_proof_data, client_proof_signature)) return fail(KEX_Event::ErrorNotrust);

            _state = KF_Done;
            return KEX_Event::OK_Done;
        }
        break;

        //No kex related packet form the server should be received in any of these states
        case NATBuster::Proto::KEXV1::K1_CHello:
        case NATBuster::Proto::KEXV1::K2_SHello:
        case NATBuster::Proto::KEXV1::K5_CIdentity:
        case NATBuster::Proto::KEXV1::KF_Err:
        default:
            return fail(KEX_Event::ErrorState);
            break;
        }
    }

    KEX::KEX_Event KEXV1_B::init_kex(Transport::OPTSession* out) {
        (void)out;
        /*if (_state == S0_New || _state == KF_Done) {
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
            _state = K1_CHello;
            return KEX_Event::OK;
        }
        _state = KF_Err;
        return KEX_Event::ErrorState;*/
        return KEX_Event::OK;
    }
}