#pragma once

#include <map>
#include <stdint.h>

#include "opt_base.h"
#include "../network/network.h"
#include "../utils/event.h"
#include "../utils/blob.h"

//Terminology used in this part of the project
//Packet: Logical blocks of data
//Frame: Induvidual 

namespace NATBuster::Common::Transport {
    struct OPTUDPSettings {
        uint16_t _max_mtu = 1500; //Max MTU of the UDP packet to send
        bool _fec_on = false; //Enable forward error correction
        Time::time_delta_type_us ping_interval = 2000000; //2sec
        Time::time_delta_type_us max_pong = 30000000; //30sec
        Time::time_delta_type_us min_retransmit = 3000; //3ms
        //Re-transmits occur after ping*ping_rt_mul + jitter*jitter_rt_mul
        float ping_rt_mul = 1.5f;
        float jitter_rt_mul = 1.5f;
        //Exponential running average weight of new data point
        float ping_average_weight = 0.01;
    };

    class OPTUDP;

    typedef std::shared_ptr<OPTUDP> OPTUDPHandle;

    class OPTUDP : public OPTBase, public std::enable_shared_from_this<OPTUDP> {
#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            static const uint32_t seq_rt_mask = 0x3f;
            static const uint32_t seq_rt_cnt = 64;
            static const uint32_t seq_num_mask = ~seq_rt_mask;

            enum PacketType : uint8_t {
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

        //These variable are only touched from the callbacks

        //To store packets that have arrived, but are not being reassambled
        std::map<uint32_t, Utils::Blob> _receive_map;
        std::list<Utils::Blob> _reassemble_list;
        //Next packet we are expecting in
        uint32_t _rx_seq = 0;
        //Last pong received
        Time::time_type_us _last_pong_in = 0;

        //These variables are locked by the tx mutex

        //The following are locked by tx_lock
        struct transmit_packet {
            transmit_packet() {

            }

            transmit_packet(transmit_packet&& other) noexcept {
                transmits = std::move(other.transmits);
                packet = std::move(other.packet);
            }

            transmit_packet& operator=(transmit_packet&& other) noexcept {
                transmits = std::move(other.transmits);
                packet = std::move(other.packet);
            }

            std::vector<Time::time_type_us> transmits;
            Utils::Blob packet;
        };
        std::list<transmit_packet> _transmit_queue;
        //Next packet to send out
        //Use the low 6 bits for a re-transmit counter
        uint32_t _tx_seq = 0;
        std::mutex _tx_lock;

        //Socket
        Network::UDPHandle _socket;
        //PollEventEmitter wrapping the socket
        std::shared_ptr<Utils::PollEventEmitter<Network::UDPHandle, Utils::Void>> _source;

        //Running average of ACK pings
        float _ping = 0.1;
        //Running average of ACK ping squares
        float _ping2 = 0.01;
        //Jitter = sqrt(ping2 - ping**2)

        //Settings objects
        OPTUDPSettings _settings;

        //Internal utility functions

        //Get outbound sequential numbers for the next n frames.
        //Returns the first number. Usable range is [retval, retval+n)
        //Once requested, an seq Must be sent, to be correctly reassambled
        inline uint32_t next_seq(int n = 1) {
            std::lock_guard lg(_tx_lock);
            uint32_t ret = _tx_seq;
            _tx_seq += packet_decoder::seq_rt_cnt * n;
            return ret;
        }

        //Calculate the delay between subsequent retransmits, based on the current ping
        inline Time::time_delta_type_us next_retransmit_delta() {
            Time::time_delta_type_us res = 1000000 * (
                _settings.ping_rt_mul * _ping +
                _settings.jitter_rt_mul * sqrt(_ping2 - _ping * _ping)
                );
            //Sum 3ms re-transmit is just spamming
            return (res < _settings.min_retransmit) ? _settings.min_retransmit : res;
        }

        inline Time::time_type_us next_floating_time() {
            {
                //Time the pong must be received by, or else the connection is declared dead
                Time::time_type_us next_pong_treshold = _last_pong_in + _settings.max_pong;

                //Time for the next scheduled re-transmit
                std::lock_guard _lg(_tx_lock);
                if (_transmit_queue.size()) {
                    packet_decoder* pkt = packet_decoder::view(_transmit_queue.front().packet);
                    uint32_t old_rt = (pkt->content.packet.seq & packet_decoder::seq_rt_mask);
                    Time::time_type_us next_transmit = _transmit_queue.front().transmits[old_rt] + next_retransmit_delta();

                    _internal_floating = (next_transmit < next_pong_treshold) ? next_transmit : next_pong_treshold;
                }
                else {
                    _internal_floating = next_pong_treshold;
                }
            }

            if (_external_floating.cb.has_function() && _external_floating.dst < _internal_floating) {
                return _external_floating.dst;
            }

            return _internal_floating;
        }

        //Send a ping packet
        void send_ping();
        //Send a pong packet, in response to a ping
        void send_pong(const Utils::ConstBlobView& ping);

        //Helper function to try to reassamble and emit as many functions as possible right now
        void try_reassemble();

        //Functions to receive events from the underlying emitter

        //Called when the emitter starts
        void on_open();
        //Called when a packet can be read
        void on_receive(Utils::Void data);
        //Called when a socket error occurs
        void on_error();
        //Called for the ping timer
        void on_ping_timer();
        //The variable timer, used for checking the pong dying, and the retransmit
        void on_floating_timer();
        Time::time_type_us _internal_floating;
        Utils::Timers::timer _external_floating;
        //Socket was closed
        void on_close();

        //Housekeeping function, to be called after all timer and packet functions
        void on_housekeeping();

        OPTUDP(
            bool is_client,
            Network::UDPHandle socket,
            OPTUDPSettings settings
        );

    public:
        OPTUDPHandle create(
            bool is_client,
            Network::UDPHandle socket,
            OPTUDPSettings settings
        );

        void start();

        //Add a callback that will be called in `delta` time, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        void addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta);

        //Add a callback that will be called at time `end`, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        void addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end);

        void updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) override;

        void send(const Utils::ConstBlobView& data);
        void sendRaw(const Utils::ConstBlobView& data);

        void close();
    };
}