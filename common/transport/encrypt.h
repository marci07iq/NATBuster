#include "../crypto/pkey.h"
#include "../crypto/cipher.h"
#include "opt_base.h"

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
    
    class EncryptOPT : public OPTBase {
        struct packet_decoder : Utils::NonStack {

            enum PacketType : uint8_t {
                //Encrypted data packet
                DATA = 0,

                //KEX packets
                KEX = 1,

                //Close packets
                CLOSE = 2
            } type;
            union {
                struct ping_data { uint8_t bytes[64]; } ping;
                struct packet_data { uint32_t seq; uint8_t data[1]; } packet;
                struct raw_data { uint8_t data[1]; } raw;
            } content;

            //Need as non const ref, so caller must maintain ownership of Packet
            static inline packet_decoder* view(Utils::BlobView& packet) {
                return (packet_decoder*)(packet.getw());
            }

            static inline const packet_decoder* cview(const Utils::ConstBlobView& packet) {
                return (const packet_decoder*)(packet.getr());
            }
        };

        static_assert(offsetof(packet_decoder, type) == 0);
        static_assert(offsetof(packet_decoder, content.ping.bytes) == 1);
        static_assert(offsetof(packet_decoder, content.packet.seq) == 1);
        static_assert(offsetof(packet_decoder, content.packet.data) == 5);
        static_assert(offsetof(packet_decoder, content.raw.data) == 1);
#pragma pack(pop)


        //The underlying transport (likely a multiplexed pipe, or OPTUDP
        std::shared_ptr<OPTBase> _underyling;
        //Public key of remote device. Give in constructor to trust.
        Crypto::PKey _remote_public;
        //Encryption system
        Crypto::CipherAES256GCMPacketStream _inbound;
        Crypto::CipherAES256GCMPacketStream _outbound;

        struct {
            //First KEX succeeed. Only emits packet to the upper layer after this
            bool _first_kex_ok : 1 = 0;
            //Enable inbound encryption
            bool _ib_enc_on : 1 = 0;
            //Enable outbound encrytpion
            bool _ob_enc_on : 1 = 0;
        };

        struct
    public:
        EncryptOPT(std::shared_ptr<OPTBase> underlying, Crypto::PKey&& remote_public) :
            _underyling(underlying),
            _remote_public(std::move(remote_public)),
            OPTBase(nullptr, nullptr, nullptr, nullptr)
        {

        }

        //Send ordered packet
        void send(const Utils::ConstBlobView& packet) {

        }

        //Send raw fasttrack packet, passed stright to the underlying inderface
        void sendRaw(const Utils::ConstBlobView& packet) {
            _underyling->sendRaw(packet);
        }
    };
}