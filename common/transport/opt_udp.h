#pragma once

#include <map>
#include <stdint.h>

#include "opt_base.h"
#include "../network/event.h"

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


    class OPTUDP : public OPTBase {
        //To store packets that have arrived, but are not being reassambled
        std::map<uint32_t, Network::Packet> _recv_list;
        std::list<Network::Packet> _reassamble_list;

        std::list<std::pair<uint64_t, Network::Packet>> _transmit_queue;

        uint32_t _tx_seq;
        uint32_t _rx_seq;

        Network::UDPEventEmitter _socket;

        uint32_t next_seq() {
            return _tx_seq++;
        }

        OPTUDP(
            OPTPacketCallback packet_callback,
            OPTRawCallback raw_callback,
            OPTClosedCallback closed_callback,
            Network::UDPHandle socket);

    public:
        OPTUDP create(
            OPTPacketCallback packet_callback,
            OPTRawCallback raw_callback,
            OPTClosedCallback closed_callback,
            Network::UDPHandle socket
        );

        void send(Network::Packet packet);
        void sendRaw(Network::Packet packet);


        void close();
    };
}