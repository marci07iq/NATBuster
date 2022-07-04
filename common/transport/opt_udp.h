#pragma once

#include <map>
#include <stdint.h>

#include "opt_base.h"
#include "../utils/event.h"

namespace NATBuster::Common::Transport {
    enum class PacketType : uint8_t {
        MGMT_HELLO = 1, //Followed by 64 byte pre-shared magic

        MGMT_PING = 2, //Followed by a 64 byte random nonce
        MGMT_PONG = 3, //Followed by the 64 byte reply

        //Packet header, 4 byte SEQ, followed by content
        PKT_MID = 16, //Middle of split packet
        PKT_START = 17, //Start of split packet
        PKT_END = 18, //End of split packet
        PKT_SINGLE = 19, //Start and end of unsplit packet

        //Packet management
        PKT_ACK = 32, //Acknowledgement, followed by 4 byte SEQ
        //PKT_FEC = 33, //???

        //For raw UDP passthrough
        UDP_PIPE = 48, //Deliver as - is
    };

#pragma pack(push, 1)
    struct packet_decoder : Utils::NonStack {
        static const uint32_t seq_rt_mask = 0x3f;
        static const uint32_t seq_rt_cnt = 64;
        static const uint32_t seq_num_mask = ~seq_rt_mask;

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

    struct OPTUDPSettings {
        uint16_t _max_mtu = 1500; //Max MTU of the UDP packet to send
        bool _fec_on = false; //Enable forward error correction
        Time::time_type_us ping_interval = 2000000; //2sec
        Time::time_type_us max_pong = 30000000; //30sec
        float ping_rt_mul = 1.5f;
        float jitter_rt_mul = 1.5f;
        //Re-transmits occur after ping*ping_rt_mul + jitter*jitter_rt_mul
        float ping_average_weight = 0.01;
    };

    class OPTUDP;

    typedef std::shared_ptr<OPTUDP> OPTUDPHandle;

    class OPTUDP : public OPTBase {
        //To store packets that have arrived, but are not being reassambled
        std::map<uint32_t, Network::Packet> _receive_map;
        std::list<Network::Packet> _reassamble_list;

        struct transmit_packet {
            std::vector<Time::time_type_us> transmits;
            Network::Packet packet;
        };
        std::list<transmit_packet> _transmit_queue;
        std::mutex _tx_lock;

        bool _got_hello = false;
        bool _run = false;
        std::mutex _system_lock;

        //Use the low 6 bits for a re-transmit counter

        //Next packet to send out
        uint32_t _tx_seq = 0;
        //Next packet we are expecting in
        uint32_t _rx_seq = 0;

        //Running average of ACK pings
        float _ping = 0.1;
        //Running average of ACK ping squares
        float _ping2 = 0.01;
        //Jitter = sqrt(ping2 - ping**2)


        OPTUDPSettings _settings;
        Network::Packet _magic;

        Network::UDPHandle _socket;

        uint32_t next_seq(int n = 1) {
            std::lock_guard lg(_tx_lock);
            uint32_t ret = _tx_seq;
            _tx_seq += packet_decoder::seq_rt_cnt * n;
            return ret;
        }

        OPTUDP(
            OPTPacketCallback packet_callback,
            OPTRawCallback raw_callback,
            OPTErrorCallback error_callback,
            OPTClosedCallback closed_callback,
            Network::UDPHandle socket,
            Network::Packet magic,
            OPTUDPSettings settings
        );

        Time::time_delta_type_us next_retransmit_delta();

        //void send_hello();

        void send_ping();
        void send_pong(Network::Packet ping);

        void try_reassemble();

        static void loop(std::weak_ptr<OPTUDP> emitter_w);
    public:
        OPTUDPHandle create(
            OPTPacketCallback packet_callback,
            OPTRawCallback raw_callback,
            OPTErrorCallback error_callback,
            OPTClosedCallback closed_callback,
            Network::UDPHandle socket,
            Network::Packet magic,
            OPTUDPSettings settings);

        void stop();

        void send(Network::Packet data);
        void sendRaw(Network::Packet data);
    };
}