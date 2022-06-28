#pragma once

#include <map>
#include <stdint.h>

#include "opt_base.h"
#include "../utils/event.h"

namespace NATBuster::Common::Transport {
    enum class PacketType {
        MGMT_HELLO = 1, //Followed by 64 byte pre-shared nonce

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

    extern "C" {
#pragma pack(push, 1)
        struct packet_header : Utils::NonStack {
            uint8_t type;
            union {
                struct ping_data { uint8_t bytes[64]; } ping;
                struct packet_data { uint32_t seq; uint8_t data[1]; } packet;
                struct raw_data { uint8_t data[1]; } raw;
            } content;
        };
#pragma pack(pop)
    };

    struct OPTUDPSettings {
        uint16_t _max_mtu = 1500; //Max MTU of the UDP packet to send
        bool _fec_on = false; //Enable forward error correction
        Time::time_type_us ping_interval = 5000000; //5sec
        Time::time_type_us max_pong = 30000000; //30sec
        float ping_rt_mul = 1.5f;
        float jitter_rt_mul = 1.5f;
        //Re-transmits occur after ping*ping_rt_mul + jitter*jitter_rt_mul
        float ping_average_weight = 0.01;
    };

    class OPTUDP : public OPTBase {
        //To store packets that have arrived, but are not being reassambled
        std::map<uint32_t, Network::Packet> _recv_list;
        std::list<Network::Packet> _reassamble_list;

        std::list<std::pair<uint64_t, Network::Packet>> _transmit_queue;

        uint32_t _tx_seq;
        uint32_t _rx_seq;

        //Running average of ACK pings
        float _ping = 0.1;
        //Running average of ACK ping squares
        float _ping2 = 0.01;
        //Jitter = sqrt(ping2 - ping**2)

        OPTUDPSettings _settings;
        Network::Packet _magic;

        Utils::SocketEventEmitter<Network::UDP, Utils::Void> _socket;


        uint32_t next_seq() {
            return _tx_seq++;
        }

    public:
        OPTUDP create(
            Network::UDPHandle socket,
            Network::Packet magic,
            OPTUDPSettings settings
        );

        bool valid();
        void close();

        bool check(Time::Timeout timeout);

        void send(Network::Packet packet);
        void sendRaw(Network::Packet packet);
    };
}