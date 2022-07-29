#pragma once

#include "punch_sym_sym.h"
#include "../common/transport/opt_udp.h"
#include "../common/transport/opt_session.h"

namespace NATBuster::Client {
    class C2Client;

    //Class to negotiate punching
    class Puncher : public std::enable_shared_from_this<Puncher> {
    public:
        using PunchCallback = Common::Utils::Callback<std::shared_ptr<Common::Transport::OPTSession>>;
    private:

        enum State : uint8_t {
            //Puncher object created
            S0_New = 0,
            //C->S hello packet sent/received
            S1_CHello = 1,
            //S->C hello packet sent/received
            S2_SHello = 2,
            //C->S start packet sent/received
            //Punching starts
            S3_CStart = 3,
            //Finished, punch successful
            SF_Success = 4,
            //Finished, punch failed
            SF_Error = 5,
        } _state = S0_New;

#pragma pack(push, 1)
        struct nat_descriptor {
            enum NATType : uint8_t {
                Forwaded = 1,
                Cone = 2,
                Symmetric = 3
            } type;
            uint16_t port_min;
            uint16_t port_max;
            uint16_t rate_limit;
        };

        struct packet_decoder : Common::Utils::NonStack {
            enum PacketType : uint8_t {
                //Self description
                PUNCH_DESCRIBE = 1,
                //Start punching
                PUNCH_START = 2,
                //Stop punching
                PUNCH_STOP = 3
            } type;
            union {
                struct {
                    nat_descriptor nat;
                    //The magic that the other side expects to receive
                    uint8_t magic[64];
                    //A PacketBlob containing the remote machines IP address/hostname as a string
                    uint8_t ip_data[1];
                } describe;
            } content;

            //Need as non const ref, so caller must maintain ownership of Packet
            static inline packet_decoder* view(Common::Utils::BlobView& packet) {
                return (packet_decoder*)(packet.getw());
            }

            static inline const packet_decoder* cview(const Common::Utils::ConstBlobView& packet) {
                return (const packet_decoder*)(packet.getr());
            }

            ~packet_decoder() = delete;
        };

        static_assert(offsetof(packet_decoder, type) == 0);
        static_assert(offsetof(packet_decoder, content.describe.nat.type) == 1);
        static_assert(offsetof(packet_decoder, content.describe.nat.port_min) == 2);
        static_assert(offsetof(packet_decoder, content.describe.nat.port_max) == 4);
        static_assert(offsetof(packet_decoder, content.describe.nat.rate_limit) == 6);
        static_assert(offsetof(packet_decoder, content.describe.magic) == 8);
        static_assert(offsetof(packet_decoder, content.describe.ip_data) == 72);
#pragma pack(pop)

        //The C2 client this puncher belongs to
        std::shared_ptr<C2Client> _c2_client;

        //The iterator to self registration
        std::list<std::shared_ptr<Puncher>>::iterator _self;

        bool _is_client;

        //The underlying comms layer to the other sides puncher
        std::shared_ptr<Common::Transport::OPTBase> _underlying;

        Common::Utils::Blob _outbound_magic_content;
        std::shared_ptr<HolepunchSym> _puncher;

        PunchCallback _punch_callback;

        Common::Crypto::PKey _self_key;
        std::shared_ptr<Common::Identity::UserGroup> _trusted_users;

        void on_open();
        void on_packet(const Common::Utils::ConstBlobView& data);
        void on_error();
        void on_close();

        void on_punch(Common::Network::UDPHandle punched);

        Puncher(
            std::shared_ptr<C2Client> c2_client,
            bool is_client,
            Common::Crypto::PKey&& self_key,
            std::shared_ptr<Common::Identity::UserGroup> trusted_users,
            std::shared_ptr<Common::Transport::OPTBase> underlying
        );

        void start();
        
        void fail();
    public:
        static std::shared_ptr<Puncher> create(
            std::shared_ptr<C2Client> c2_client,
            bool is_client,
            Common::Crypto::PKey&& self_key,
            std::shared_ptr<Common::Identity::UserGroup> trusted_users,
            std::shared_ptr<Common::Transport::OPTBase> underlying
        );

        inline void set_punch_callback(PunchCallback::raw_type punch_callback) {
            _punch_callback = punch_callback;
        }

        void close();

    };
};