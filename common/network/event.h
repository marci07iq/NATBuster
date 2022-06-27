#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <numeric>

#include "network.h"

namespace NATBuster::Common::Network {
    typedef std::function<void(TCPSHandle)> TCPSEventCallback;
    typedef std::function<void(TCPCHandle)> TCPCEventCallback;
    typedef std::function<void(UDPHandle)> UDPEventCallback;

    class TCPSEventEmitter {
        const TCPSEventCallback _listen_callback;
        const TCPSEventCallback _error_callback;
        std::list<TCPSHandle> _sockets;
        std::thread _recv_thread;
        bool _run;
        std::mutex _lock;

        //Keeps looping until:
        //-Stopped
        //-Abandoned
        //-No open sockets left
        static void loop(std::weak_ptr<TCPSEventEmitter> emitter_w);

        TCPSEventEmitter(
            const std::list<TCPSHandle>& sockets,
            TCPSEventCallback listen_callback,
            TCPSEventCallback error_callback);

        void stop();

    public:
        static std::shared_ptr<TCPSEventEmitter> create(const std::vector<TCPSHandle>& sockets);
    };



    class TCPCEventEmitter {
        const TCPCEventCallback _read_callback;
        const TCPCEventCallback _error_callback;
        std::list<TCPCHandle> _sockets;
        std::thread _recv_thread;
        bool _run;
        std::mutex _lock;

        //Keeps looping until:
        //-Stopped
        //-Abandoned
        //-No open sockets left
        static void loop(std::weak_ptr<TCPCEventEmitter> emitter_w);

        TCPCEventEmitter(
            const std::list<TCPCHandle>& sockets,
            TCPCEventCallback listen_callback,
            TCPCEventCallback error_callback);

        void stop();

    public:
        static std::shared_ptr<TCPCEventEmitter> create(const std::vector<TCPCHandle>& sockets);
    };



    class UDPEventEmitter {
        const UDPEventCallback _read_callback;
        const UDPEventCallback _error_callback;
        std::list<UDPHandle> _sockets;
        std::thread _recv_thread;
        bool _run;
        std::mutex _lock;

        //Keeps looping until:
        //-Stopped
        //-Abandoned
        //-No open sockets left
        static void loop(std::weak_ptr<UDPEventEmitter> emitter_w);

        UDPEventEmitter(
            const std::list<UDPHandle>& sockets,
            UDPEventCallback listen_callback,
            UDPEventCallback error_callback);

        void stop();

    public:
        static std::shared_ptr<UDPEventEmitter> create(const std::vector<UDPHandle>& sockets);
    };
};