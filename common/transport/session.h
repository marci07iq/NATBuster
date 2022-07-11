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

    class Session : public OPTBase, public std::enable_shared_from_this<Session> {
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
        std::shared_ptr<OPTBase> _underlying;
        //Encryption system
        Crypto::CipherAES256GCMPacketStream _inbound;
        Crypto::CipherAES256GCMPacketStream _outbound;

        std::unique_ptr<Proto::KEX> _kex;

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
        void on_packet(const Utils::ConstBlobView& data);
        //Called when a packet can be read
        void on_raw(const Utils::ConstBlobView& data);
        //Called when a socket error occurs
        void on_error();
        //Socket was closed
        void on_close();

    public:
        Session(
            bool is_client,
            std::shared_ptr<OPTBase> underlying,
            Crypto::PKey&& my_private,
            Crypto::PKey&& remote_public
        );

        void start();

        //Send ordered packet
        void send(const Utils::ConstBlobView& packet);

        //Send raw fasttrack packet, passed stright to the underlying inderface
        void sendRaw(const Utils::ConstBlobView& packet);

        void close();
    };
}