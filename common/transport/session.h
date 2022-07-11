#pragma once

#include <memory>

#include "../crypto/pkey.h"
#include "../crypto/cipher.h"
#include "../utils/callbacks.h"
#include "opt_base.h"

#include "kex.h"

namespace NATBuster::Common::Proto {
    class KEXV1;
    class KEXV1_A;
    class KEXV1_B;
}

namespace NATBuster::Common::Transport {
    //Client: The initiator of the connection
    //Server: The destination of the connection
    //Assumed that server has a whitelist of all known client keys
    //Assumed that client should have the server key

    // a: Initiator, b : Target
    // P : Public key, S : (Secret)Private key
    // N : Nonces
    // LT, DH: Long term and DH keys

    // A->B: M1: Version
    // A<-B: M2: Version and capabilities
    // Selected version is min(Va, Vb)

    class EncryptOPT : public OPTBase, public std::enable_shared_from_this<EncryptOPT> {
        enum PacketType : uint8_t {
            //Encrypted data packet
            DATA = 0,

            //KEX packets
            KEX = 1,

            //Remote side no longer wishes to send data encrypted
            //Generally a bad idea, unless performance needed
            //
            ENC_OFF = 2,

            //Close packets
            CLOSE = 3
        };
        //The underlying transport (likely a multiplexed pipe, or OPTUDP
        std::shared_ptr<OPTBase> _underyling;
        //Public key of remote device. Give in constructor to trust.
        Crypto::PKey _remote_public;
        //Encryption system
        Crypto::CipherAES256GCMPacketStream _inbound;
        Crypto::CipherAES256GCMPacketStream _outbound;

        Proto::KEX* _kex;

        struct {
            //First KEX succeeed. Only emits packet to the upper layer after this
            bool _first_kex_ok : 1 = false;
            //Enable inbound encryption
            bool _enc_on_ib : 1 = false;
            //Enable outbound encrytpion
            bool _enc_on_ob : 1 = false;
            //Enable ENC_OFF
            //If false, remote side can't turn off encryption
            bool _enc_off_enable : 1 = false;
            //Encrypt raw data stream
            //TODO
            bool _enc_raw : 1 = false;
        } _flags;

        friend class Proto::KEXV1_A;
        friend class Proto::KEXV1_B;

        //Called when a packet can be read
        void on_packet(const Utils::ConstBlobView& data) {
            //Decrypt data if there is encyption
            Utils::Blob data_decrypted;
            Utils::Blob no_aad;

            if (_flags._enc_on_ib) {
                _inbound.decrypt(data, data_decrypted, no_aad);
            }

            //Data to use in the rest of the process
            const Utils::ConstBlobView& data_dc = _flags._enc_on_ib ? data_decrypted : data;


            PacketType type = (PacketType)(*((uint8_t*)data_dc.getr()));

            Utils::ConstBlobSliceView content = data_dc.cslice_right(1);

            switch (type)
            {
            case NATBuster::Common::Transport::EncryptOPT::DATA:
                if (_flags._first_kex_ok) {
                    _result_callback(data);
                }
                else {
                    //Error;
                    close();
                }
                break;
            case NATBuster::Common::Transport::EncryptOPT::KEX:
            {
                Proto::KEX::KEX_Event res = _kex->recv(content, this);
                switch (res)
                {
                case NATBuster::Common::Proto::KEX::KEX_Event::OK:
                    break;
                case NATBuster::Common::Proto::KEX::KEX_Event::OK_Done:
                    _flags._first_kex_ok = true;
                    break;
                case NATBuster::Common::Proto::KEX::KEX_Event::ErrorGeneric:
                case NATBuster::Common::Proto::KEX::KEX_Event::ErrorMalformed:
                case NATBuster::Common::Proto::KEX::KEX_Event::ErrorNotrust:
                case NATBuster::Common::Proto::KEX::KEX_Event::ErrorCrypto:
                case NATBuster::Common::Proto::KEX::KEX_Event::ErrorSend:
                case NATBuster::Common::Proto::KEX::KEX_Event::ErrorState:
                default:
                    close();
                    break;
                }
            }
            break;
            case NATBuster::Common::Transport::EncryptOPT::ENC_OFF:
                if (_flags._enc_off_enable) {
                    _flags._enc_on_ib = false;
                }
                else {
                    close();
                }
                break;
            case NATBuster::Common::Transport::EncryptOPT::CLOSE:
                break;
            default:
                break;
            }
        }
        //Called when a packet can be read
        void on_raw(const Utils::ConstBlobView& data) {
            if (_flags._first_kex_ok) {
                _raw_callback(data);
            }
        }
        //Called when a socket error occurs
        void on_error() {
            _error_callback();
        }
        //Socket was closed
        void on_close() {
            _close_callback();
        }

    public:
        EncryptOPT(std::shared_ptr<OPTBase> underlying, Crypto::PKey&& remote_public) :
            _underyling(underlying),
            _remote_public(std::move(remote_public)),
            OPTBase()
        {

        }

        void start() {
            _underyling->set_packet_callback(new Utils::MemberCallback<EncryptOPT, void, const Utils::ConstBlobView&>(weak_from_this(), &EncryptOPT::on_packet));
            _underyling->set_raw_callback(new Utils::MemberCallback<EncryptOPT, void, const Utils::ConstBlobView&>(weak_from_this(), &EncryptOPT::on_raw));
            _underyling->set_error_callback(new Utils::MemberCallback<EncryptOPT, void>(weak_from_this(), &EncryptOPT::on_error));
            _underyling->set_close_callback(new Utils::MemberCallback<EncryptOPT, void>(weak_from_this(), &EncryptOPT::on_close));

            _underyling->start();
        }

        //Send ordered packet
        void send(const Utils::ConstBlobView& packet) {
            Utils::Blob packet_content;

        }

        //Send raw fasttrack packet, passed stright to the underlying inderface
        void sendRaw(const Utils::ConstBlobView& packet) {
            _underyling->sendRaw(packet);
        }

        void close() {
            _underyling->close();
        }
    };
}