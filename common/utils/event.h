#pragma once

//#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <list>
#include <mutex>
#include <numeric>
#include <optional>
#include <shared_mutex>
//#include <queue>
//#include <variant>

#include "callbacks.h"
#include "time.h"

namespace NATBuster::Common::Utils
{
    //Abstract class to provide an interruptable delay
    //Can also be used to poll for other forms of events, and emit those
    class EventEmitterProvider {
    protected:
        EventEmitterProvider() {

        }
    public:
        //Bind the emitter to the current thread
        virtual void bind() = 0;


        //Only call from same thread used to bind
        //<0: Infinte
        //0: No wait, but can still do housekeeping
        //>0: Intended wait period.
        //Will return ASAP after interrupt
        //May return sooner for other reasons
        virtual void wait(Time::time_delta_type_us delay) = 0;

        //Only call from same thread used to bind
        //Run all runnow jobs, and any housekeeping
        //May be implemented as a 0 lenght wait
        virtual void yield() {
            wait(0);
        }

        //Can call from any thread
        //Make wait return ASAP
        virtual void interrupt() = 0;

        //Can call from any thread
        //Stop waiting, and make same thread run fn ASAP
        virtual void run_now(Common::Utils::Callback<>::raw_type fn) = 0;

        virtual ~EventEmitterProvider() {

        }
    };

    class EventEmitter : Utils::SharedOnly<EventEmitter> {
    public:
        using StartupCallback = Utils::Callback<>;
        using TimerCallback = Utils::Callback<>;
        using ShutdownCallback = Utils::Callback<>;
    private:
        struct timer {
            TimerCallback cb;
            Time::time_type_us expiry;

            bool active = false;
            std::list<std::shared_ptr<timer>>::iterator self;

            timer(TimerCallback::raw_type cb_, Time::time_type_us expiry_) :
                cb(cb_), expiry(expiry_), active(false) {
            }
        };

    public:
        using timer_hwnd = std::weak_ptr<timer>;

    private:
        StartupCallback _callback_startup;
        ShutdownCallback _callback_shutdown;

        //OS dependant provider of delay (and maybe other events)
        std::unique_ptr<EventEmitterProvider> _delay;

        //List of timers to add
        std::list<std::shared_ptr<timer>> _timer_additions;
        //List of timers to add
        std::list<std::weak_ptr<timer>> _timer_deletions;
        //Protects timer changes
        std::mutex _timer_lock;

        //External flag to keep running
        bool _running;
        //Thread if running in async mode
        std::thread _thread;
        std::mutex _system_lock;

        void run_now(Common::Utils::Callback<>::raw_type fn) {
            _delay->run_now(fn);
        }

        //Looping function
        static void loop(std::shared_ptr<EventEmitter> obj) {
            //Take ownership of the delay
            obj->_delay->bind();

            //Issue start callback
            obj->_callback_startup();

            //By the time we shut down, the object may not exist
            //So make a copy of the shutdown callback
            ShutdownCallback shutdown_cb_backup;
            shutdown_cb_backup.move_from_safe_other(obj->_callback_shutdown);

            //No need to keep timers in the object, as they are only 
            std::list<std::shared_ptr<timer>> timers;

            //Only hold a weak self ref, to detect of the object was dropped
            std::weak_ptr<EventEmitter> objw = obj;
            do {
                //Check the running flag
                {
                    std::lock_guard _lg(obj->_system_lock);
                    if (!obj->_running) break;
                }

                //Execute the timer updates
                {
                    std::lock_guard _lg(obj->_timer_lock);
                    //Activate all elements (to signal being in the main list)
                    for (auto&& it : obj->_timer_additions) {
                        it->active = true;
                    }
                    //Move entire additions list in
                    //All interators in splice remain valid, so no work to do with self
                    timers.splice(timers.end(), obj->_timer_additions);
                    //Execute deletions
                    for (auto&& it : obj->_timer_deletions) {
                        std::shared_ptr<timer> its = it.lock();
                        if (its) {
                            assert(its->active);
                            timers.erase(its->self);
                            its->self = timers.end();
                        }
                    }
                    //Wipe deletions list
                    obj->_timer_deletions.clear();
                }

                //Run expired timers
                bool has_next_timer = false;
                Time::time_type_us next_timer;
                {
                    auto it = timers.begin();
                    while (it != timers.end()) {
                        if ((*it)->expiry <= Time::now()) {
                            (*it)->cb.call_and_clear();
                            auto it2 = it++;
                            timers.erase(it2);
                        }
                        else {
                            if (!has_next_timer || (*it)->expiry < next_timer) {
                                has_next_timer = true;
                                next_timer = (*it)->expiry;
                            }
                            ++it;
                        }
                    }
                }

                Time::time_delta_type_us delay = 
                    has_next_timer ?
                    std::max<Time::time_delta_type_us>(0, next_timer - Time::now()) :
                    Time::TIME_DELTA_INFINTE;

                obj->_delay->wait(delay);

                //Keep updating the shutdown copy to the latest possible value
                shutdown_cb_backup.move_from_safe_other(obj->_callback_shutdown);
                obj.reset();
                obj = objw.lock();
            } while (obj);

            //Final task: call the shutdown backup, if set
            shutdown_cb_backup();
        }

        EventEmitter() {

        }
    public:
        //Start on this thread (consumes thread)
        void start_sync(std::unique_ptr<EventEmitterProvider> delay) {
            {
                std::lock_guard _lg(_system_lock);
                assert(_running == false);
                _running = true;
                _delay = delay;
            }
            loop(shared_from_this());
        }

        //Starts on a new thread
        void start_async(std::unique_ptr<EventEmitterProvider> delay) {
            std::lock_guard _lg(_system_lock);
            assert(_running == false);
            _running = true;
            _delay = delay;
            _thread = std::thread(EventEmitter::loop, shared_from_this());
        }

        //Can call from any thread
        timer_hwnd add_timer(TimerCallback::raw_type cb, Time::time_type_us expiry) {
            std::shared_ptr<timer> new_timer;
            //Create new timer object
            {
                std::lock_guard _lg(_timer_lock);
                std::shared_ptr<timer> new_timer = std::make_shared<timer>(cb, expiry);
                new_timer->self = _timer_additions.insert(_timer_additions.end(), new_timer);
            }

            //Wake up the delay, to process the new addition
            _delay->interrupt();

            return new_timer;
        }

        //Can call from any thread
        timer_hwnd add_delay(TimerCallback::raw_type cb, Time::time_delta_type_us delay) {
            return add_timer(cb, Time::now() + delay);
        }

        //Can call from any thread
        bool cancel_timer(timer_hwnd hwnd) {
            std::shared_ptr shwnd = hwnd.lock();
            if (shwnd) {
                //Read only lock (to get end)
                std::lock_guard _lg(_timer_lock);
                //If the interator was already added, add it to the cancel request list
                if (shwnd->active) {
                    _timer_deletions.push_back(hwnd);
                    //Wake up the executor, to process the deletion
                    //_delay->interrupt();
                }
                //If not yet added: Safely remove form add list
                else {
                    _timer_additions.erase(shwnd->self);
                }
            }
        }

        //Can call from any thread
        void stop() {
            {
                std::lock_guard _lg(_system_lock);
                _running = false;
            }
            _delay->interrupt();
        }

        void join() {
            _thread.join();
        }
        void detach() {
            _thread.detach();
        }

        ~EventEmitter() {

        }
    };

    /*enum class PollResponseType {
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
        //{ (int)5 } -> std::same_as<int>;
        //Validity check. If false, the event emitter shuts down
        { t->is_valid() } -> std::same_as<bool>;
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

            Time::time_type_us dst = 0;

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
            while ((_timers.size() > 0) && (_timers.top()->dst <= Time::now())) {
                _timers.top()->cb();
                delete _timers.top();
                _timers.pop();
            }
            //Call floating timer
            //Multiple times (if the user keeps re-setting it)
            while (_floating_timer.cb.has_function() && _floating_timer.dst <= Time::now()) {
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
        using OpenCallback = Utils::Callback<>;
        using ResultCallback = Utils::Callback<RESULT_TYPE>;
        using ErrorCallback = Utils::Callback<>;
        using CloseCallback = Utils::Callback<>;

    protected:
        OpenCallback _open_callback = nullptr;
        ResultCallback _result_callback = nullptr;
        ErrorCallback _error_callback = nullptr;
        CloseCallback _close_callback = nullptr;

    protected:
        EventEmitter() {

        }

    public:
        //Called after start, before any other callback
        //All callbacks are issued from the same thread
        //Safe to call from any thread, even during a callback
        inline void set_open_callback(
            OpenCallback::raw_type open_callback) {
            _open_callback = open_callback;
        }

        //Called when a result is found on the poll source
        //All callbacks are issued from the same thread
        //Safe to call from any thread, even during a callback
        inline void set_result_callback(
            ResultCallback::raw_type result_callback) {
            _result_callback = result_callback;
        }

        //Called when an error is issued from the poll source
        //All callbacks are issued from the same thread
        //Safe to call from any thread, even during a callback
        inline void set_error_callback(
            ErrorCallback::raw_type error_callback) {
            _error_callback = error_callback;
        }

        //Called when the poll source has closed, or the event emitter object has been deleted
        //All callbacks are issued from the same thread
        //The source object may no longer exist if you don't have a shared_ptr to it
        //Safe to call from any thread, even during a callback
        inline void set_close_callback(
            CloseCallback::raw_type close_callback = nullptr) {
            _close_callback = close_callback;
        }

        //Start the event emitter
        virtual void start() = 0;

        //Add a callback that will be called in `delta` time, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        virtual void addDelay(Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) = 0;

        //Add a callback that will be called at time `end`, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        virtual void addTimer(Timers::TimerCallback::raw_type cb, Time::time_type_us end) = 0;

        //Overwrite the next time the floating timer is fired
        //There is only one floating timer
        //Only call from callbacks, or before start
        virtual void updateFloatingNext(Timers::TimerCallback::raw_type cb, Time::time_type_us end) = 0;

        //Close the event emitter
        //Will issue a close callback unless one has already been issued
        virtual void close() = 0;

        virtual ~EventEmitter() {

        }
    };



    //Wrap an object satisfying the Pollable constaint
    //A dedicated thread is used to
    //Emits `event_callback` when an event occurs
    //Emits `error_callback` when an error occurs
    //`close` marks the event thread for closing, and closes the underyling poll source
    //If the underlying source closes, the thread stops
    //Once all references to the object are abandoned, it self destructs
    //Timers should only be set from any of the event callbacks, or before start
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
        //-Source becomes invalid
        static void loop(std::unique_ptr<std::shared_ptr<PollEventEmitter>> emitter_i);

    public:

        PollEventEmitter(const POLL_SRC& source);

        //Start the event emitter
        void start();

        //Add a callback that will be called in `delta` time, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        virtual void addDelay(Timers::TimerCallback::raw_type cb, Time::time_delta_type_us delta) override {
            _timers.addDelay(cb, delta);
        }

        //Add a callback that will be called at time `end`, if the emitter is still running
        //There is no way to cancel this call
        //Only call from callbacks, or before start
        virtual void addTimer(Timers::TimerCallback::raw_type cb, Time::time_type_us end) override {
            _timers.addTimer(cb, end);
        }

        //Overwrite the next time the floating timer is fired
        //There is only one floating timer
        //Only call from callbacks, or before start
        virtual void updateFloatingNext(Timers::TimerCallback::raw_type cb, Time::time_type_us end) override {
            _timers.updateFloatingNext(cb, end);
        }

        //Close the event emitter
        //Will issue a close callback unless one has already been issued
        void close();

        void detach() {
            _thread.detach();
        }

        void join() {
            _thread.join();
        }

        virtual ~PollEventEmitter() {
            if (_thread.get_id() == std::this_thread::get_id()) {
                //The thread is cleaning itself up
                //It should stop itself after the current callback is done
                _thread.detach();
            }
            else {
                _thread.join();
            }
        }
    };

    template<typename POLL_SRC, typename RESULT_TYPE>
        requires Pollable<POLL_SRC, RESULT_TYPE>
    void PollEventEmitter<POLL_SRC, RESULT_TYPE>::loop(std::unique_ptr<std::shared_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>>> emitter_u) {

        typename EventEmitter<RESULT_TYPE>::CloseCallback callback_backup = nullptr;

        std::weak_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>> emitter_w;

        //Thread startup
        {
            //Retrieve the shared ptr
            std::shared_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>> emitter = *emitter_u;
            //Destroy the unique ptr
            emitter_u.reset();

            std::lock_guard lg(emitter->_lock);

            //Backup the close callback
            //Since this (may) be called after the object is already gone
            callback_backup.move_from_safe_other(emitter->_close_callback);

            //Only issue open callback if the socket isnt totally dead
            if (emitter->_source->is_valid()) {
                //Start open callback
                emitter->_open_callback();
            }

            emitter_w = emitter;
            //Release all hold to the shared_ptr.
            //From here on, the thread keeps re-locking the weak ptr, to chekc if the object was abandoned
        }

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

            if (!emitter->_source->is_valid()) break;

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
        }

        callback_backup();
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

        //Note: unique_ptr<shared_ptr<>> is used to make sure that no lingering copy of the shared pointer being pass in
        //is kept somewhere in the thread till the thread function returns.
        //Since thread ctor moves its arguments, this should happen just by using shared_ptr, but this is safer

        _thread = std::thread(
            PollEventEmitter<POLL_SRC, RESULT_TYPE>::loop,
            std::make_unique<std::shared_ptr<PollEventEmitter<POLL_SRC, RESULT_TYPE>>>(this->shared_from_this())
        );
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