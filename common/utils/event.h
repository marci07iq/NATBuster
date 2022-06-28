#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <numeric>
#include <optional>

#include "time.h"
#include "timeout.h"

namespace NATBuster::Common::Utils
{
    template <typename U>
    concept EventResponse = requires(const U & u)
    {
        { !!u } -> std::same_as<bool>;
    };

    template <typename T, typename U>
    concept Eventable = EventResponse<U> && requires(const T & t, Timeout to)
    {
        //Validity check. If false, the event emitter returns
        { t.valid() } -> std::same_as<bool>;
        //Close function, for when the event emitter needs to exit
        { t.close() } -> std::same_as<void>;
        //Gets the next response
        { t.check(to) } -> std::same_as <U>;
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
        std::mutex _lock;

        // Keeps looping until:
        //-Stopped
        //-Abandoned
        //-No open sockets left
        static void loop(std::weak_ptr<SocketEventEmitter> emitter_w);

        SocketEventEmitter(
            const EVENT_SRC& source,

            EventCallback event_callback,
            EventCallback error_callback,
            TimerEventCallback timer_callback,
            ShutdownEventCallback shutdown_callback);

        void stop();

    public:
        static std::shared_ptr<SocketEventEmitter> create(const EVENT_SRC& source);
    };

    template<typename EVENT_SRC, typename RESULT_TYPE>
    void SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::loop(std::weak_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> emitter_w) {

        //By the time shutdown callback is called, the object could be dead
        //So copy it out now
        ShutdownEventCallback shutdown_callback_copy;
        {
            std::shared_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> emitter = emitter_w.lock();
            if (!emitter) return;
            shutdown_callback_copy = emitter->_shutdown_callback;
        }

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

            /*//Filter for dead sockets
            emitter->_sockets.remove_if(
                [emitter](TCPSHandle handle) {
                    if (!handle->valid()) {
                        emitter->_error_callback(handle);
                        return true;
                    }
                    return false;
                });*/

            if (!emitter->_source.valid()) break;

            //Find server with event
            RESULT_TYPE result = SOCK_HWND::find(emitter->_source, );

            if (!!result) {
                emitter->_event_callback(result);
            }
        }

        shutdown_callback_copy();

        std::cout << "Listen collector exited" << std::endl;
    }

    template<typename EVENT_SRC, typename RESULT_TYPE>
    SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::SocketEventEmitter(
        const EVENT_SRC& source,
        EventCallback event_callback,
        EventCallback error_callback,
        TimerEventCallback timer_callback,
        ShutdownEventCallback shutdown_callback) :
        _source(source),
        _event_callback(event_callback),
        _error_callback(error_callback),
        _timer_callback(timer_callback),
        _shutdown_callback(shutdown_callback) {
        _run = true;
    }

    template<typename EVENT_SRC, typename RESULT_TYPE>
    void SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::stop() {
        std::lock_guard lg(_lock);
        _run = false;
    }

    template<typename EVENT_SRC, typename RESULT_TYPE>
    std::shared_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> SocketEventEmitter<EVENT_SRC, RESULT_TYPE>::create(const EVENT_SRC& sockets) {
        std::shared_ptr<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>> obj =
            make_shared<SocketEventEmitter<EVENT_SRC, RESULT_TYPE>>(sockets);

        obj->_recv_thread = std::thread(SocketEventEmitter<EVENT_SRC>::loop, obj);

        return obj;
    }
};