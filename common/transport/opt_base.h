#pragma once

#include <functional>

#include "../error/codes.h"
#include "../utils/blob.h"
#include "../utils/callbacks.h"
#include "../utils/copy_protection.h"
#include "../utils/event.h"
#include "../utils/math.h"

#include "../identity/identity.h"

namespace NATBuster::Transport {
    //Ordered packet transport
    class OPTBase : Utils::NonCopyable {
    public:
        using OpenCallback = Utils::Callback<>;
        using PacketCallback = Utils::Callback<const Utils::ConstBlobView&>;
        using ErrorCallback = Utils::Callback<ErrorCode>;
        using CloseCallback = Utils::Callback<>;

        using timer_hwnd = Utils::EventEmitter::timer_hwnd;
        using TimerCallback = Utils::EventEmitter::TimerCallback;
    protected:
        OpenCallback _callback_open;
        PacketCallback _callback_packet;
        PacketCallback _callback_raw;
        ErrorCallback _callback_error;
        CloseCallback _callback_close;
        
        bool _is_client;

        //Abstact base class
        OPTBase(bool is_client);

    public:
        //Called when the event amitter is ready, and can accept send
        //Thread safe
        inline void set_callback_open(
            OpenCallback::raw_type callback_open) {
            _callback_open = callback_open;
        }
        //Called when ordered packet arrives
        //Thread safe
        inline void set_callback_packet(
            PacketCallback::raw_type callback_packet) {
            _callback_packet = callback_packet;
        }
        //Called when raw packet arrives
        inline void set_callback_raw(
            PacketCallback::raw_type callback_raw) {
            _callback_raw = callback_raw;
        }
        //Called when error occurs
        inline void set_callback_error(
            ErrorCallback::raw_type callback_error) {
            _callback_error = callback_error;
        }
        //Called when system closes
        inline void set_callback_close(
            CloseCallback::raw_type callback_close) {
            _callback_close = callback_close;
        }

        //Send ordered packet
        //Safe to call from any thread, even during a callback
        virtual void send(const Utils::ConstBlobView& packet) = 0;
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        //Safe to call from any thread, even during a callback
        virtual void sendRaw(const Utils::ConstBlobView& packet) = 0;

        //Can call from any thread
        virtual timer_hwnd add_timer(TimerCallback::raw_type cb, Time::time_type_us expiry) = 0;

        //Can call from any thread
        virtual timer_hwnd add_delay(TimerCallback::raw_type cb, Time::time_delta_type_us delay) = 0;

        //Can call from any thread
        virtual void cancel_timer(timer_hwnd hwnd) = 0;

        //Can call from any thread
        virtual std::shared_ptr<Identity::User> getUser() {
            return Identity::User::Anonymous;
        }

        //Can call from any thread
        inline bool is_client() const {
            return _is_client;
        }

        virtual uint32_t get_mtu_normal() = 0;
        virtual uint32_t get_mtu_raw() = 0;

        virtual void start() = 0;

        //Close connection (gracefully) if possible
        //Will issue a close callback unless one has already been issued
        //Safe to call from any thread, even during a callback
        virtual void close() = 0;

        inline void close_error(ErrorCode code) {
            _callback_error(code);
            close();
        }

        virtual ~OPTBase() {

        }
    };

    /*//Stacked filter
    class OPTBaseFilter : public OPTBase {
    protected:
        std::shared_ptr<OPTBase> _underlying;

        OPTBaseFilter(bool is_client, std::shared_ptr<OPTBase> underlying) :
            OPTBase(is_client) {

        }
    private:
        using Utils::EventEmitter<const Utils::ConstBlobView&>::set_result_callback;
    public:
        //Add a callback that will be called in `delta` time, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        virtual void addDelay(Utils::Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) {
            _underlying->addDelay(cb, delta);
        }

        //Add a callback that will be called at time `end`, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        virtual void addTimer(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
            _underlying->addTimer(cb, end);
        }

        //Overwrite the next time the floating timer is fired
        //There is only one floating timer
        //Only call from callbacks, or before start
        virtual void updateFloatingNext(Utils::Timers::TimerCallback::raw_type cb, Time::time_type_us end) {
            _underlying->updateFloatingNext(cb, end);
        }

        //Send ordered packet
        virtual void send(const Utils::ConstBlobView& packet) override {
            _underlying->send(packet);
        };
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        virtual void sendRaw(const Utils::ConstBlobView& packet) override {
            _underlying->send(packet);
        };
        //Close connection (gracefully) if possible
        virtual void close() override {
            _underlying->close();
        }

        virtual std::shared_ptr<Identity::User> getUser() override {
            _underlying->getUser();
        }
    };*/
};