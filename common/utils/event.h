#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <numeric>
#include <optional>
#include <variant>

#include "time.h"

namespace NATBuster::Common::Utils
{
    enum class EventResponseType {
        OK,
        //An error occured during the blocking call
        SystemError,
        //System error, but a return value is given (error occured with a specific return value)
        SpecificError,
        //Only given from impossible code paths
        UnexpectedError,
        //The wait timed out
        Timeout
    };

    template <typename V>
    class EventResponse {
        const EventResponseType _type;
        V _value;
    public:
        EventResponse(EventResponseType type, const V& value = V()) :
            _type(type),
            _value(value) {
        }

        inline bool timeout() {
            return _type == EventResponseType::Timeout;
        }

        inline bool error() {
            return (_type == EventResponseType::SystemError) || (_type == EventResponseType::SpecificError) || (_type == EventResponseType::UnexpectedError);
        }

        inline bool ok() {
            return(_type == EventResponseType::OK);
        }

        inline bool has() {
            return (_type == EventResponseType::OK) || (_type == EventResponseType::SpecificError);
        }

        inline V& get() {
            return _value;
        }
    };

    struct Void {

    };

    template <>
    class EventResponse<Void> {
        const EventResponseType _type;
        Void _v;
    public:
        EventResponse(EventResponseType type) :
            _type(type) {
        }

        inline bool timeout() {
            return _type == EventResponseType::Timeout;
        }

        inline bool error() {
            return (_type == EventResponseType::SystemError) || (_type == EventResponseType::SpecificError) || (_type == EventResponseType::UnexpectedError);
        }

        inline bool ok() {
            return(_type == EventResponseType::OK);
        }

        inline bool has() {
            return (_type == EventResponseType::OK) || (_type == EventResponseType::SpecificError);
        }

        inline Void& get() {
            return _v;
        }
    };

    template <typename SRC, typename VAL>
    concept Eventable = requires(SRC t, Time::Timeout to)
    {
        //Validity check. If false, the event emitter returns
        { t.valid() } -> std::same_as<bool>;
        //Close function, for when the event emitter needs to exit
        { t.close() } -> std::same_as<void>;
        //Gets the next response
        { t.check(to) } -> std::same_as<EventResponse<VAL>>;
    };

    //Wrap an object satisfying the 
    //Emits `event_callback` when there is a read/accept available
    //Emits `error_callback` when a socket has an error (and was closed)
    //Emits `timer_callback` every `interval`. Set interval to zero to disable
    //Stop closes all associated sockets.
    //If stop called, no more active sockets, or SocketEventEmitter object abandoned -> shuts down
    //Calls `shutdown_callback`
    template<typename EVENT_SRC, typename RESULT_TYPE>
        requires Eventable<EVENT_SRC, RESULT_TYPE>
    class SocketEventEmitter
    {
        using EventCallback = std::function<void(std::shared_ptr<SocketEventEmitter>, RESULT_TYPE)>;
        using TimerEventCallback = std::function<void(std::shared_ptr<SocketEventEmitter>)>;
        using ShutdownEventCallback = std::function<void()>;

        const EventCallback _event_callback;
        const EventCallback _error_callback;
        const TimerEventCallback _timer_callback;
        const ShutdownEventCallback _shutdown_callback;

        EVENT_SRC _source;
        std::thread _recv_thread;
        bool _run;
        const Time::time_delta_type_us _period;
        std::mutex _lock;

        // Keeps looping until:
        //-Stopped
        //-Abandoned
        //-Source bebomes invalid
        static void loop(std::weak_ptr<SocketEventEmitter> emitter_w);

        SocketEventEmitter(
            const EVENT_SRC& source,
            EventCallback event_callback,
            EventCallback error_callback = nullptr,
            TimerEventCallback timer_callback = nullptr,
            ShutdownEventCallback shutdown_callback = nullptr,
            Time::time_delta_type_us period = Time::TIME_DELTA_INFINTE);

        void stop();

    public:
        static std::shared_ptr<SocketEventEmitter> create(const EVENT_SRC& source);
    };

    template<typename EVENT_SRC, typename RESULT_TYPE>
        requires Eventable<EVENT_SRC, RESULT_TYPE>
    void SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::loop(std::weak_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> emitter_w) {

        //By the time shutdown callback is called, the object could be dead
        //So copy it out now
        ShutdownEventCallback shutdown_callback_copy;
        {
            std::shared_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> emitter = emitter_w.lock();
            if (!emitter) return;
            shutdown_callback_copy = emitter->_shutdown_callback;
        }

        Time::time_type_us next_timer = Time::now() + _period;

        while (true) {
            //Check if object still exists
            std::shared_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> emitter = emitter_w.lock();
            if (!emitter) break;

            //Check exit condition
            {
                std::lock_guard lg(emitter->_lock);
                //Check exit condition
                if (!emitter->_run) break;
            }

            if (!emitter->_source.valid()) break;

            Time::time_delta_type_us timeout_us = (Time::time_delta_type_us)next_timer - (Time::time_delta_type_us)Time::now();


            Time::Timeout to(
                (_period < 0) ?
                (Time::TIME_DELTA_INFINTE) :
                ((timeout_us < 0) ? 0 : timeout_us));

            //Find server with event
            EventResponse<RESULT_TYPE> result = emitter->_source->check(to);

            if (result.timeout()) {
                next_timer = Time::now();
                emitter->_timer_callback();
            }
            if (result.error()) {
                emitter->_error_callback(result.get());
            }
            if (result.ok()) {
                emitter->_event_callback(result.get());
            }

        }

        shutdown_callback_copy();

        //std::cout << "Listen collector exited" << std::endl;
    }

    template<typename EVENT_SRC, typename RESULT_TYPE>
        requires Eventable<EVENT_SRC, RESULT_TYPE>
    SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::SocketEventEmitter(
        const EVENT_SRC& source,
        EventCallback event_callback,
        EventCallback error_callback,
        TimerEventCallback timer_callback,
        ShutdownEventCallback shutdown_callback,
        Time::time_delta_type_us period) :
        _source(source),
        _event_callback(event_callback),
        _error_callback(error_callback),
        _timer_callback(timer_callback),
        _shutdown_callback(shutdown_callback),
        _period(period) {
        _run = true;
    }

    template<typename EVENT_SRC, typename RESULT_TYPE>
        requires Eventable<EVENT_SRC, RESULT_TYPE>
    void SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::stop() {
        std::lock_guard lg(_lock);
        _source.close(); //TODO: lock _source
        _run = false;
    }

    template<typename EVENT_SRC, typename RESULT_TYPE>
        requires Eventable<EVENT_SRC, RESULT_TYPE>
    std::shared_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::create(const EVENT_SRC& sockets) {
        std::shared_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> obj =
            make_shared<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>>(sockets);

        obj->_recv_thread = std::thread(SocketEventEmitter<EVENT_SRC>::loop, obj);

        return obj;
    }
};