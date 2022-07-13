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
    class OPTTCP;

    typedef std::shared_ptr<OPTTCP> OPTTCPHandle;

    class OPTTCP : public OPTBase, public std::enable_shared_from_this<OPTTCP> {
#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            enum PacketType : uint8_t {
                PKT_NORMAL,
                PKT_RAW
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
        };

        static_assert(offsetof(packet_decoder, type) == 0);
        static_assert(offsetof(packet_decoder, len) == 1);
        static_assert(offsetof(packet_decoder, data) == 5);
#pragma pack(pop)
        
        //Socket
        Network::TCPCHandle _socket;
        //PollEventEmitter wrapping the socket
        std::shared_ptr<Utils::PollEventEmitter<Network::TCPCHandle, Utils::Void>> _source;

        uint32_t _reassamble_total_len;
        std::list<Utils::Blob> _reassamble_list;

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
            if (len <= _reassamble_total_len) {
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
        //Called when a packet can be read
        void on_receive(Utils::Void data);
        //Called when a socket error occurs
        void on_error();
        //Socket was closed
        void on_close();

        OPTTCP(
            bool is_client,
            Network::TCPCHandle socket
        );

    public:
        static OPTTCPHandle create(
            bool is_client,
            Network::TCPCHandle socket);

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