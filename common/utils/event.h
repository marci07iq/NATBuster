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
#include "waker.h"
#include "shared_unique.h"

namespace NATBuster::Utils
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

        //Can call from any thread
        //Make wait returns and run fn ASAP
        //No guarantee made on order, fn may run only when wait is called again
        virtual void run_now(Utils::Callback<>::raw_type fn) = 0;

        //Can call from any thread
        //Make wait return ASAP
        virtual void interrupt() {
            run_now(nullptr);
        }

        virtual ~EventEmitterProvider() {

        }
    };

    class EventEmitter : public Utils::SharedOnly<EventEmitter> {
        friend Utils::SharedOnly<EventEmitter>;
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
        shared_unique_ptr<EventEmitterProvider> _delay;

        //List of timers to add
        std::list<std::shared_ptr<timer>> _timer_additions;
        //List of timers to add
        std::list<std::weak_ptr<timer>> _timer_deletions;
        //Protects timer changes
        std::mutex _timer_lock;

        OnetimeWaker _waker;

        //External flag to keep running
        bool _running = false;
        //Thread if running in async mode
        std::thread _thread;
        std::mutex _system_lock;

        void run_now(Utils::Callback<>::raw_type fn) {
            _delay->run_now(fn);
        }

        //Looping function
        static void loop(std::shared_ptr<EventEmitter> obj) {
            //Take ownership of the delay
            obj->_delay->bind();

            obj->_waker.wake();

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
                Time::time_type_us next_timer = 0;
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

        EventEmitter() : _running(false) {

        }
    public:
        //Start on this thread (consumes thread)
        void start_sync(shared_unique_ptr<EventEmitterProvider> delay) {
            {
                std::lock_guard _lg(_system_lock);
                assert(_running == false);
                _running = true;
                _delay = std::move(delay);
            }
            loop(shared_from_this());
        }

        //Starts on a new thread
        void start_async(shared_unique_ptr<EventEmitterProvider> delay) {
            std::lock_guard _lg(_system_lock);
            assert(_running == false);
            _running = true;
            _delay = std::move(delay);
            _thread = std::thread(EventEmitter::loop, shared_from_this());
            _waker.wait();
        }

        //Can call from any thread
        timer_hwnd add_timer(TimerCallback::raw_type cb, Time::time_type_us expiry) {
            std::shared_ptr<timer> new_timer;
            //Create new timer object
            {
                std::lock_guard _lg(_timer_lock);
                new_timer = std::make_shared<timer>(cb, expiry);
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
        void cancel_timer(timer_hwnd hwnd) {
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

};