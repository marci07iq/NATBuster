#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <numeric>

#include "network.h"

namespace NATBuster::Common::Network
{

    //Wrap a collection of sockets into an event_emitter.
    //Emits `read_callback` when there is a read/accept available
    //Emits `error_callback` when a socket has an error (and was closed)
    //Emits `timer_callback` every `interval`. Set interval to zero to disable
    //Stop closes all associated sockets.
    template<typename SOCK_WNDD>
    class SocketEventEmitter
    {
        using SocketEventCallback = std::function<void(SOCK_WNDD)>;
        using TimerEventCallback = std::function<void()>;
        using ShutdownEventCallback = std::function<void()>;

        const SocketEventCallback _read_callback;
        const SocketEventCallback _error_callback;
        const TimerEventCallback _timer_callback;
        const ShutdownEventCallback _shutdown_callback;

        std::list<SOCK_WNDD> _sockets;
        std::thread _recv_thread;
        bool _run;
        std::mutex _lock;

        // Keeps looping until:
        //-Stopped
        //-Abandoned
        //-No open sockets left
        static void loop(std::weak_ptr<SocketEventEmitter> emitter_w);

        TCPSEventEmitter(
            const std::list<SOCK_HWND> &sockets,
            SocketEventCallback listen_callback,
            SocketEventCallback error_callback);

        void stop();

    public:
        static std::shared_ptr<TCPSEventEmitter> create(const std::vector<TCPSHandle> &sockets);
    };
};