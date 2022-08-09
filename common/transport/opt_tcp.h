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

namespace NATBuster::Transport {
    class OPTTCP;

    typedef std::shared_ptr<OPTTCP> OPTTCPHandle;

    class OPTTCP : public OPTBase, public Utils::SharedOnly<OPTTCP> {
        friend Utils::SharedOnly<OPTTCP>;
#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            enum PacketType : uint8_t {
                PKT_NORMAL = 1,
                PKT_RAW = 2
            } type;
            uint32_t len;
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
        static_assert(offsetof(packet_decoder, len) == 1);
        static_assert(offsetof(packet_decoder, data) == 5);
#pragma pack(pop)
        
        //Socket
        std::shared_ptr<Utils::EventEmitter> _emitter;
        Network::TCPCHandleS _socket;
        
        static const uint32_t max_packet_size = 1 << 24;

        uint32_t _reassamble_total_len = 0;
        std::list<Utils::Blob> _reassamble_list;

        std::mutex _out_lock;

        bool skip_from_reassamble_list(uint32_t len) {
            if (len <= _reassamble_total_len) {
                uint32_t progress = 0;
                
                auto it = _reassamble_list.begin();

                //More left to read
                while (progress < len) {
                    //_reassamble_total_len must have lied.
                    //Fatal bug
                    if (it == _reassamble_list.end()) {
                        throw 1;
                    }

                    //Amount still needed to copy
                    uint32_t next_frag = len - progress;

                    //Copy no more than available
                    if (it->size() < next_frag) {
                        next_frag = it->size();
                    }

                    //Decrese readable size of next blob
                    it->resize(next_frag, it->size());
                    _reassamble_total_len -= next_frag;

                    //If no more left in this blob, advance
                    if (it->size() == 0) {
                        _reassamble_list.pop_front();
                    }
                    //Stuff can only be left in the blob if we are done reading
                    else {
                        assert(progress == len);
                    }
                    it = _reassamble_list.begin();
                }
                return true;
            }
            return false;
        }
        bool read_from_reassamble_list(Utils::Blob& dst, uint32_t len, bool peek) {
            if (len  <= _reassamble_total_len) {
                uint32_t progress = 0;
                dst.resize(len);

                auto it = _reassamble_list.begin();

                //More left to read
                while (progress < len) {
                    //_reassamble_total_len must have lied.
                    //Fatal bug
                    if (it == _reassamble_list.end()) {
                        throw 1;
                    }

                    //Amount still needed to copy
                    uint32_t next_frag = len - progress;
                    
                    //Copy no more than available
                    if (it->size() < next_frag) {
                        next_frag = it->size();
                    }

                    //Copy out of buffer
                    dst.copy_from(it->cslice_left(next_frag), progress);
                    progress += next_frag;

                    if (peek) {
                        ++it;
                    }
                    else {
                        //Decrese readable size of next blob
                        it->resize(next_frag, it->size());
                        _reassamble_total_len -= next_frag;

                        //If no more left in this blob, advance
                        if (it->size() == 0) {
                            _reassamble_list.pop_front();
                        }
                        //Stuff can only be left in the blob if we are done reading
                        else {
                            assert(progress == len);
                        }
                        it = _reassamble_list.begin();
                    }
                }
                return true;
            }
            return false;
        }

        //Functions to receive events from the underlying emitter

        //Called when the emitter starts
        void on_open();
        //Called when a packet arrives
        void on_receive(const Utils::ConstBlobView&);
        //Called when a socket error occurs
        void on_error(ErrorCode code);
        //Socket was closed
        void on_close();

        OPTTCP(
            bool is_client,
            std::shared_ptr<Utils::EventEmitter> emitter,
            Network::TCPCHandleS socket
        );

    public:
        void start();

        inline timer_hwnd add_timer(TimerCallback::raw_type cb, Time::time_type_us expiry) {
            return _emitter->add_timer(cb, expiry);
        }
        inline timer_hwnd add_delay(TimerCallback::raw_type cb, Time::time_delta_type_us delay) {
            return _emitter->add_delay(cb, delay);
        }
        inline void cancel_timer(timer_hwnd hwnd) {
            return _emitter->cancel_timer(hwnd);
        }

        void send(const Utils::ConstBlobView& data);
        void sendRaw(const Utils::ConstBlobView& data);

        inline uint32_t get_mtu_normal() {
            return max_packet_size;
        }
        inline uint32_t get_mtu_raw() {
            return max_packet_size;
        }

        void close();

        virtual ~OPTTCP() {

        }
    };
}