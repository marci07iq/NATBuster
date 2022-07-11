#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <variant>

#include "callbacks.h"
#include "time.h"

namespace NATBuster::Common::Utils
{
    enum class PollResponseType {
        //Poll succeeded, data is returned
        OK,
        //The poll wait expired
        Timeout,
        //Event emitter should shut down
        ClosedError,
        //Unknown Error
        UnknownError,
    };

    template <typename V>
    class PollResponse {
    public:
        using Type = PollResponseType;
    private:
        const PollResponseType _type;
        V _value;
    public:
        PollResponse(Type type, V&& value = V()) :
            _type(type),
            _value(std::move(value)) {
        }

        inline bool timeout() {
            return _type == Type::Timeout;
        }

        inline bool error() {
            return (_type == Type::ClosedError) || (_type == Type::UnknownError);
        }

        inline bool ok() {
            return(_type == Type::OK);
        }

        inline bool has() {
            return (_type == Type::OK);
        }

        inline V& get() {
            assert(has());
            return _value;
        }
    };



    struct Void {
        inline Void() {

        }
    };



    template <typename SRC, typename VAL>
    concept Pollable = requires(SRC t, Time::Timeout to)
    {
        //Validity check. If false, the event emitter shuts down
        { t->valid() } -> std::same_as<bool>;
        //Close function, for when the event emitter needs to exit
        { t->close() } -> std::same_as<void>;
        //Gets the next response, for a maximum timeout
        { t->check(to) } -> std::same_as<PollResponse<VAL>>;
    };



    class Timers {
    public:
        using TimerCallback = Utils::Callback<>;
        struct timer {
            //Priority queue needs all elements to be const once they are in
            //But cb is not used in sorting, so doesn't have to be
            mutable TimerCallback cb;

            Time::time_type_us dst;

            bool operator<(const timer& rhs) {
                return dst < rhs.dst;
            }
            bool operator>(const timer& rhs) {
                return dst > rhs.dst;
            }
            bool operator<=(const timer& rhs) {
                return dst <= rhs.dst;
            }
            bool operator>=(const timer& rhs) {
                return dst >= rhs.dst;
            }
        };

    private:
        std::priority_queue<timer*> _timers;

        timer _floating_timer;
    public:
        void addDelay(TimerCallback::raw_type cb, Time::time_delta_type_us delta) {
            _timers.push(new timer{
                .cb = cb,
                .dst = Time::now() + delta
                });
        }

        void addTimer(TimerCallback::raw_type cb, Time::time_type_us end) {
            _timers.push(new timer{
                .cb = cb,
                .dst = end
                });
        }

        void updateFloatingNext(TimerCallback::raw_type cb, Time::time_type_us end) {
            _floating_timer.cb = cb;
            _floating_timer.dst = end;
        }

        Time::time_delta_type_us run() {
            //Call all one time timers
            while ((_timers.size() > 0) && (_timers.top()->dst >= Time::now())) {
                _timers.top()->cb();
                delete _timers.top();
                _timers.pop();
            }
            if (_timers.size() == 0) {
                //No timers left, next timer is in infinte time
                return Time::TIME_DELTA_INFINTE;
            }
            //Call floating timer
            //Multiple times (if the user keeps re-setting it)
            while (_floating_timer.cb.has_function() && _floating_timer.dst < Time::now()) {
                _floating_timer.cb.call_and_clear();
            }

            if (_timers.size() > 0) {
                //Delta to the next scheduled timer
                Time::time_delta_type_us delta =
                    (Time::time_delta_type_us)_timers.top()->dst -
                    (Time::time_delta_type_us)Time::now();

                //Delta to the floating timer (if one is set)
                if (_floating_timer.cb.has_function()) {
                    Time::time_delta_type_us floating_delta =
                        (Time::time_delta_type_us)_floating_timer.dst -
                        (Time::time_delta_type_us)Time::now();
                }

                //Dont return negative timeout (since that means infinte)
                return (delta < 0) ? 0 : delta;
            }

            //No scheduled timer
            //Delta to the floating timer (if one is set)
            if (_floating_timer.cb.has_function()) {
                Time::time_delta_type_us floating_delta =
                    (Time::time_delta_type_us)_floating_timer.dst -
                    (Time::time_delta_type_us)Time::now();
                return (floating_delta < 0) ? 0 : floating_delta;
            }

            //No normal or floating timer set.
            return Time::TIME_DELTA_INFINTE;
        }
    };



    template<typename RESULT_TYPE>
    class EventEmitter : Utils::NonCopyable {
    public:
        using ResultCallback = Utils::Callback<RESULT_TYPE>;
        using ErrorCallback = Utils::Callback<>;
        using CloseCallback = Utils::Callback<>;

    protected:
        ResultCallback _result_callback = nullptr;
        ErrorCallback _error_callback = nullptr;
        CloseCallback _close_callback = nullptr;

    protected:
        EventEmitter() {

        }

    public:
        void set_result_callback(
            ResultCallback::raw_type result_callback) {
            _result_callback = result_callback;
        }

        void set_error_callback(
            ErrorCallback::raw_type error_callback) {
            _error_callback = error_callback;
        }

        void set_close_callback(
            CloseCallback::raw_type close_callback = nullptr) {
            _close_callback = close_callback;
        }

        //Start the event emitter
        virtual void start() = 0;

        virtual void addDelay(Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) = 0;

        virtual void addTimer(Timers::TimerCallback::raw_type cb, Time::time_type_us end) = 0;

        virtual void updateFloatingNext(Timers::TimerCallback::raw_type cb, Time::time_type_us end) = 0;

        virtual void close() = 0;
    };



    //Wrap an object satisfying the Pollable constaint
    //Emits `event_callback` when an event occurs
    //Emits `error_callback` when an error occurs
    //`close` marks the event thread for closing, and closes the underyling poll source
    //If the underlying source closes, the thread stops
    //Once all references to the object are abandoned, it self destructs
    //Timers should only be set from any of the event callbacks.
    template<typename POLL_SRC, typename RESULT_TYPE>
        requires Pollable<POLL_SRC, RESULT_TYPE>
    class PollEventEmitter : public EventEmitter<RESULT_TYPE>, public std::enable_shared_from_this<PollEventEmitter<POLL_SRC, RESULT_TYPE>>
    {
    private:
        POLL_SRC _source;
        std::thread _thread;

        //The following are protected by the mutex
        bool _run = false;
        std::mutex _lock;

        Timers _timers;

        // Keeps looping until:
        //-Stopped
        //-Abandoned
        //-Source bebomes invalid
        static void loop(std::weak_ptr<PollEventEmitter> emitter_w);

    public:
        PollEventEmitter(const POLL_SRC& source);
        //static std::shared_ptr<PollEventEmitter> create(const POLL_SRC& source);

        void start();

        virtual void addDelay(Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) override {
            _timers.addDelay(cb, delta);
        }

        virtual void addTimer(Timers::TimerCallback::raw_type cb, Time::time_type_us end) override {
            _timers.addTimer(cb, end);
        }

        virtual void updateFloatingNext(Timers::TimerCallback::raw_type cb, Time::time_type_us end) override {
            _timers.updateFloatingNext(cb, end);
        }

        void close();
    };

    template<typename POLL_SRC, typename RESULT_TYPE>
        requires Pollable<POLL_SRC, RESULT_TYPE>
    void PollEventEmitter<POLL_SRC, RESULT_TYPE>::loop(std::weak_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>> emitter_w) {

        //By the time shutdown callback is called, the object could be dead
        //So copy it out now
        while (true) {
            //Check if object still exists
            std::shared_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>> emitter = emitter_w.lock();
            if (!emitter) break;

            //Check exit condition
            {
                std::lock_guard lg(emitter->_lock);
                //Check exit condition
                if (!emitter->_run) break;
            }

            if (!emitter->_source->valid()) break;

            //Run all timers
            Time::time_delta_type_us timeout_us = emitter->_timers.run();

            Time::Timeout to(timeout_us);

            //Check the source (waiting max till the next timeout is scheduled)
            PollResponse<RESULT_TYPE> result = emitter->_source->check(to);

            if (result.error()) {
                emitter->_error_callback();
            }
            if (result.ok()) {
                emitter->_result_callback(result.get());
            }

            //No more strong refs
            if (emitter.use_count() == 1) {
                emitter->_close_callback();
            }
        }


        //std::cout << "Listen collector exited" << std::endl;
    }

    template<typename POLL_SRC, typename RESULT_TYPE>
        requires Pollable<POLL_SRC, RESULT_TYPE>
    PollEventEmitter<POLL_SRC, RESULT_TYPE>::PollEventEmitter(
        const POLL_SRC& source) :
        EventEmitter<RESULT_TYPE>(),
        _source(source) {

    }

    template<typename POLL_SRC, typename RESULT_TYPE>
        requires Pollable<POLL_SRC, RESULT_TYPE>
    void PollEventEmitter<POLL_SRC, RESULT_TYPE>::start() {
        std::lock_guard lg(_lock);
        //Mark thread allowed
        _run = true;

        _thread = std::thread(PollEventEmitter<POLL_SRC, RESULT_TYPE>::loop, this->weak_from_this());
    }

    template<typename POLL_SRC, typename RESULT_TYPE>
        requires Pollable<POLL_SRC, RESULT_TYPE>
    void PollEventEmitter<POLL_SRC, RESULT_TYPE>::close() {
        std::lock_guard lg(_lock);
        //Mark thread for stopiing
        _run = false;
        //Close underyling
        _source->close();
    }

    /*template<typename POLL_SRC, typename RESULT_TYPE>
        requires Pollable<POLL_SRC, RESULT_TYPE>
    std::shared_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>> PollEventEmitter<POLL_SRC, RESULT_TYPE>::create(const POLL_SRC& sockets) {

        std::shared_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>> obj =
            std::make_shared<PollEventEmitter<POLL_SRC, RESULT_TYPE>>(sockets);

        obj->_self = obj;

        return obj;
    }*/
};