#pragma once

#include <map>
#include <memory>
#include <shared_mutex>

#include "opt_base.h"

namespace NATBuster::Common::Transport {
    class OPTPipes;

    class OPTPipe : public OPTBase, public std::enable_shared_from_this<OPTPipe> {
        std::shared_ptr<OPTPipes> _underlying;
        uint32_t _id;

        Utils::Timers::timer _floating;

        friend class OPTPipes;

        enum OpenState {
            None,
            OpenRequsted,
            Opened,
            CloseRequrested,
            Closed
        } _open_state;

    private:
        OPTPipe(
            bool is_client,
            std::shared_ptr<OPTPipes> underlying,
            uint32_t id);

        static std::shared_ptr<OPTPipe> create(
            bool is_client,
            std::shared_ptr<OPTPipes> underlying,
            uint32_t id);
    public:
        void start();

        //Send ordered packet
        void send(const Utils::ConstBlobView& packet);
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        void sendRaw(const Utils::ConstBlobView& packet);

        //Add a callback that will be called in `delta` time, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        void addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta);

        //Add a callback that will be called at time `end`, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        void addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end);

        //Overwrite the next time the floating timer is fired
        //There is only one floating timer
        //Only call from callbacks, or before start
        void updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end);

        //Close connection (gracefully) if possible
        void close();

        std::shared_ptr<Identity::User> getUser() override;
    };

    class OPTPipes : public Utils::EventEmitter<std::shared_ptr<OPTPipe>>, public std::enable_shared_from_this<OPTPipes> {
    private:

#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            static const uint32_t seq_rt_mask = 0x3f;
            static const uint32_t seq_rt_cnt = 64;
            static const uint32_t seq_num_mask = ~seq_rt_mask;

            enum PacketType : uint8_t {
                //Request to open a new pipe with ID
                //Resposne comes via MGMT_OPEN_RESP or MGMT_CLOSE
                MGMT_OPEN_REQ = 1, //Followed by 4 byte pipe ID
                //Request to open a pipe with ID approved
                MGMT_OPEN_RESP = 2, //Followed by 4 byte pipe ID
                //Close the pipe with ID
                MGMT_CLOSE = 3, //Followed by 4 byte pipe ID

                DATA = 16, //Followed by 4 byte pipe ID, and data
            } type;
            uint32_t id;
            union {
                struct { uint8_t data[1]; } data;
            } content;

            //Need as non const ref, so caller must maintain ownership of Packet
            static inline packet_decoder* view(Utils::BlobView& packet);

            static inline const packet_decoder* cview(const Utils::ConstBlobView& packet);
        };

        static_assert(offsetof(packet_decoder, type) == 0);
        static_assert(offsetof(packet_decoder, id) == 1);
        static_assert(offsetof(packet_decoder, content.data.data) == 5);
#pragma pack(pop)


        std::shared_ptr<OPTBase> _underlying;

        std::map<uint32_t, std::shared_ptr<OPTPipe>> _pipes;
        //Opened pipes are odd if true
        //Client is the side that initiated the connection
        bool _is_client;
        uint32_t _next_pipe_id;
        //Unique lock when changing the map contents
        //Shared lock when only changing the elements
        std::shared_mutex _pipe_lock;

        Utils::Timers::timer _floating;

        friend class OPTPipe;

        //Called when a packet can be read
        void on_open();
        //Called when a packet can be read
        void on_packet(const Utils::ConstBlobView& data);
        //Called when a packet can be read
        void on_raw(const Utils::ConstBlobView& data);
        //Called when a socket error occurs
        void on_error();
        //Timer
        void on_floating() {
            std::shared_lock _lg(_pipe_lock);

            //Find earliest
            for (auto&& it : _pipes) {
                if (it.second->_floating.dst < Time::now()) {
                    it.second->_floating.cb.call_and_clear();
                }
            }

            if (_floating.dst < Time::now()) {
                _floating.cb.call_and_clear();
            }
           

            updateFloatingNext();
        }
        //Socket was closed
        void on_close();

        void updateFloatingNext() {
            std::shared_lock _lg(_pipe_lock);

            int cnt = 0;
            Time::time_type_us earliest = 0;

            //Find earliest amound pipes
            for (auto&& it : _pipes) {
                if (it.second->_floating.cb.has_function()) {
                    if (cnt == 0) {
                        earliest = it.second->_floating.dst;
                    }
                    else {
                        earliest = Time::earlier(earliest, it.second->_floating.dst);
                    }
                    ++cnt;
                }
            }

            //Add own
            if (_floating.cb.has_function()) {
                if (cnt == 0) {
                    earliest = _floating.dst;
                }
                else {
                    earliest = Time::earlier(earliest, _floating.dst);
                }
                ++cnt;
            }

            //Set underlying callback
            if (cnt > 0) {
                _underlying->updateFloatingNext(
                    new Utils::MemberCallback<OPTPipes, void>(
                        weak_from_this(),
                        &OPTPipes::on_floating
                        ), earliest);
            }
        };

        void closePipe(uint32_t id);
    public:
        OPTPipes(bool is_client);

        void start();

        //Opened pipe mist be started to actually initiate the connection
        std::shared_ptr<OPTPipe> openPipe();

        //Add a callback that will be called in `delta` time, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        void addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta);

        //Add a callback that will be called at time `end`, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        void addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end);

        //Overwrite the next time the floating timer is fired
        //There is only one floating timer
        //Only call from callbacks, or before start
        void updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end);

        //Close the event emitter
        //Will issue a close callback unless one has already been issued
        void close();
    };
};