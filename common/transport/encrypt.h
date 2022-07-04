#include "../crypto/pkey.h"
#include "opt_base.h"

namespace NATBuster::Common::Transport {

    enum class EncryptedPacketType {
        Control = 1,
        Data = 2,
        Raw = 3
    };

#pragma pack(push, 1)
    struct packet_decoder : Utils::NonStack {
        uint8_t type;
        union {
            struct ping_data { uint8_t bytes[64]; } ping;
            struct packet_data { uint32_t seq; uint8_t data[1]; } packet;
            struct raw_data { uint8_t data[1]; } raw;
        } content;

        //Need as non const ref, so caller must maintain ownership of Packet
        static inline packet_decoder* view(Network::Packet& packet) {
            return (packet_decoder*)(packet.get());
        }
    };

    static_assert(offsetof(packet_decoder, type) == 0);
    static_assert(offsetof(packet_decoder, content.ping.bytes) == 1);
    static_assert(offsetof(packet_decoder, content.packet.seq) == 1);
    static_assert(offsetof(packet_decoder, content.packet.data) == 5);
    static_assert(offsetof(packet_decoder, content.raw.data) == 1);
#pragma pack(pop)

    class EncryptOPT : public OPTBase {
        //The underlying transport (likely a multiplexed pipe, or OPTUDP
        std::shared_ptr<OPTBase> _underyling;
        //Public key of remote device. Give in constructor to trust.
        Crypto::PKey _remote_public;
    public:
        EncryptOPT(std::shared_ptr<OPTBase> underlying, Crypto::PKey&& remote_public) : _underyling(underlying), _remote_public(std::move(remote_public)) {

        }

        //Send ordered packet
        void send(Network::Packet packet) = 0;
        
        void sendRaw(Network::Packet packet) = 0;
    };
}