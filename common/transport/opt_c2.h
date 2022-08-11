#include "opt_pipes.h"

namespace NATBuster::Transport {
    namespace C2Main {
#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            enum PacketType : uint8_t {
                //C->S
                //Login to the C2 server. Provide machine name, list of friend fingerprints
                //Auth is already done by the underlying OPTSession
                LOGIN = 1,

                //S->C
                //Status of the remots
                //Information will be given about any client who listed this is a friend
                WELCOME = 2,

                //S->C
                //Friend joined the network
                ON_JOIN = 16,
                //S->C
                //Friend left the network
                ON_QUIT = 17,
            } type;
            uint8_t data[1];

            //Need as non const ref, so caller must maintain ownership of Packet
            static inline packet_decoder* view(Utils::BlobView& packet) {
                return (packet_decoder*)(packet.getw());
            }

            static inline const packet_decoder* cview(const Utils::ConstBlobView& packet) {
                return (const packet_decoder*)(packet.getr());
            }

            ~packet_decoder() = delete;
        };

        static_assert(offsetof(packet_decoder, type) == 0);
        static_assert(offsetof(packet_decoder, data) == 1);
#pragma pack(pop)
    }

    namespace C2PipeRequest {
#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            enum PacketType : uint8_t {
                PIPE_USER = 1
            } type;
            uint8_t data[1];

            //Need as non const ref, so caller must maintain ownership of Packet
            static inline packet_decoder* view(Utils::BlobView& packet) {
                return (packet_decoder*)(packet.getw());
            }

            static inline const packet_decoder* cview(const Utils::ConstBlobView& packet) {
                return (const packet_decoder*)(packet.getr());
            }

            ~packet_decoder() = delete;
        };

        static_assert(offsetof(packet_decoder, type) == 0);
        static_assert(offsetof(packet_decoder, data) == 1);
#pragma pack(pop)
    }
}