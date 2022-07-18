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

        //Floating timer set by upstream
        Utils::Timers::timer _floating;

        friend class OPTPipes;

        enum OpenState {
            //Freshly created client pipe
            None,
            //C->S open request sent
            OpenRequstSent,
            //S->C open request received
            OpenRequstRecv,
            //Pipe opened
            Opened,
            Closed
        } _open_state;

        //A timer set when the open request is sent. Closes the object once done
        void on_open_expired();

    private:
        OPTPipe(
            bool is_client,
            std::shared_ptr<OPTPipes> underlying,
            uint32_t id);

        static std::shared_ptr<OPTPipe> create(
            bool is_client,
            std::shared_ptr<OPTPipes> underlying,
            uint32_t id);

        void erase();
    public:
        //Use to accept a server pipe, or actually open the client pipe
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

        //Close connection gracefully
        //If you dont want to accept a pipe, simple drop the pointer
        //Or call drop
        void close();

        //Drop the connection
        //Equivalent to just releasing referenes
        //Notifies other side of closing the connection only if it is open
        void drop();

        std::shared_ptr<Identity::User> getUser() override;

        virtual ~OPTPipe();
    };

    struct OPTPipeOpenData {
        std::shared_ptr<OPTPipe> pipe;
        const Utils::_ConstBlobView& content;
    };

    class OPTPipes : public OPTBase, public std::enable_shared_from_this<OPTPipes> {
    public:
        using PipeCallback = Utils::Callback<OPTPipeOpenData>;
    private:

        PipeCallback _pipe_callback;

#pragma pack(push, 1)
        struct packet_decoder : Utils::NonStack {
            
            enum PacketType : uint8_t {
                //Request to open a new pipe with ID
                //Resposne comes via MGMT_OPEN_RESP or MGMT_CLOSE
                //Followed by 4 byte pipe ID, and any additional data specific to the overload
                PIPE_OPEN_REQ = 1,
                //Request to open a pipe with ID approved
                //Followed by 4 byte pipe ID
                PIPE_OPEN_RESP = 2,
                //Close the pipe with ID
                PIPE_CLOSE = 3, //Followed by 4 byte pipe ID

                PIPE_DATA = 4, //Followed by 4 byte pipe ID, and data

                SELF_DATA = 5,
            } type;
            union {
                struct { uint32_t id; uint8_t data[1]; } pipe;
                struct { uint8_t data[1]; } data;
            } content;

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
        static_assert(offsetof(packet_decoder, content.pipe.id) == 1);
        static_assert(offsetof(packet_decoder, content.pipe.data) == 5);
        static_assert(offsetof(packet_decoder, content.data.data) == 1);
#pragma pack(pop)


        std::shared_ptr<OPTBase> _underlying;

        //Collection of all known pipes
        std::map<uint32_t, std::weak_ptr<OPTPipe>> _pipes;
        //Opened pipes are odd if true
        //Client is the side that initiated the connection
        uint32_t _next_pipe_id;
        //Unique lock when changing the map contents
        //Shared lock when only changing the elements
        std::shared_mutex _pipe_lock;

        std::mutex _tx_lock;

        Utils::Timers::timer _floating;

        friend class OPTPipe;
    private:

        //Called when a packet can be read
        void on_open();
        //Called when a packet can be read
        void on_packet(const Utils::ConstBlobView& data);
        //Called when a packet can be read
        void on_raw(const Utils::ConstBlobView& data);
        //Called when a socket error occurs
        void on_error();
        //Timer
        void on_floating();
        //Socket was closed
        void on_close();

        void updateFloatingNext() {
            std::shared_lock _lg(_pipe_lock);

            int cnt = 0;
            Time::time_type_us earliest = 0;

            //Find earliest amound pipes
            for (auto&& it : _pipes) {
                std::shared_ptr<OPTPipe> pipe = it.second.lock();
                if (pipe) {
                    if (pipe->_floating.cb.has_function()) {
                        if (cnt == 0) {
                            earliest = pipe->_floating.dst;
                        }
                        else {
                            earliest = Time::earlier(earliest, pipe->_floating.dst);
                        }
                        ++cnt;
                    }
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
                    new Utils::MemberWCallback<OPTPipes, void>(
                        weak_from_this(),
                        &OPTPipes::on_floating
                        ), earliest);
            }
        };

        OPTPipes(
            bool is_client,
            std::shared_ptr<OPTBase> underlying);
    public:
        static std::shared_ptr<OPTPipes> create(
            bool is_client,
            std::shared_ptr<OPTBase> underlying);

        void start();

        //Opened pipe must be started to actually initiate the connection
        std::shared_ptr<OPTPipe> openPipe();

        //Called when a UDP type packet is available
        //All callbacks are issued from the same thread
        //Safe to call from any thread, even during a callback
        inline void set_pipe_callback(
            PipeCallback::raw_type pipe_callback) {
            _pipe_callback = pipe_callback;
        }

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

        //Send ordered packet
        void send(const Utils::ConstBlobView& packet) {
            Utils::Blob full_packet = Utils::Blob::factory_empty(1 + packet.size());

            OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
            header->type = OPTPipes::packet_decoder::SELF_DATA;
            
            full_packet.copy_from(packet, 1);

            std::lock_guard _lg(_tx_lock);

            _underlying->sendRaw(full_packet);
        }
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        void sendRaw(const Utils::ConstBlobView& packet) {
            Utils::Blob full_packet = Utils::Blob::factory_empty(1 + packet.size());

            OPTPipes::packet_decoder* header = OPTPipes::packet_decoder::view(full_packet);
            header->type = OPTPipes::packet_decoder::SELF_DATA;

            full_packet.copy_from(packet, 1);

            std::lock_guard _lg(_tx_lock);

            _underlying->sendRaw(full_packet);
        }

        std::shared_ptr<Identity::User> getUser() override;

        //Close the event emitter
        //Will issue a close callback unless one has already been issued
        void close();

        virtual ~OPTPipes() {

        }
    };
};