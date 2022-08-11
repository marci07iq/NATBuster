#pragma once

#ifdef WIN32

#define NOMINMAX

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2ipdef.h>
#include <windns.h>
#include <mswsock.h>

#include <iphlpapi.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#undef NOMINMAX

#include <array>
#include <stdint.h>
#include <string>

#include "network.h"

namespace NATBuster::Network {

    struct EventHandleOSData {
        HANDLE hwnd;
    };

    struct AddrInfoOSData : public addrinfo {
        
    };

    static_assert(sizeof(AddrInfoOSData) == sizeof(addrinfo));

    //Network address OS defined implementation
    //Represent an address (IP and port), IPV4 or IPV6
    class NetworkAddressOSData {
    public:
        SOCKADDR_STORAGE _address;

        INT _address_length = sizeof(_address);

        NetworkAddressOSData();
        NetworkAddressOSData(NetworkAddressOSData& other);
        NetworkAddressOSData& operator=(NetworkAddressOSData& other);

        inline const SOCKADDR_STORAGE* get_data() const {
            return (SOCKADDR_STORAGE*)&_address;
        }
        inline SOCKADDR_STORAGE* get_dataw() {
            return (SOCKADDR_STORAGE*)&_address;
        }

        inline const INT size() const {
            return _address_length;
        }
        inline INT* sizew() {
            return &_address_length;
        }
    };

    std::ostream& operator<<(std::ostream& os, const NetworkAddressOSData& addr);

    //RAII move-only wrapper for OS sockets
    class SocketOSData : Utils::NonCopyable {
        SOCKET _socket = INVALID_SOCKET;
    public:
        SocketOSData(SOCKET socket = INVALID_SOCKET);
        SocketOSData(SocketOSData&& other) noexcept;
        SocketOSData& operator=(SocketOSData&& other) noexcept;
        inline void set(SOCKET socket);

        inline SOCKET get();

        inline bool is_valid() const;
        inline bool is_invalid() const;

        void set_events(HANDLE& hwnd);

        inline void close();
        ~SocketOSData();
    };

    class SocketEventEmitterProviderImpl : Utils::NonCopyable {
        //Variables consumed by the thread

        std::vector<HANDLE> _socket_events;
        std::vector<std::shared_ptr<SocketEventHandle>> _socket_objects;
        std::mutex _sockets_lock;

        //Fast to access variables to send data to thread

        HANDLE _this_thread = INVALID_HANDLE_VALUE;
        std::list<Utils::Callback<>> _tasks;
        std::list<std::shared_ptr<SocketEventHandle>> _added_socket_objects;
        std::list<std::shared_ptr<SocketEventHandle>> _closed_socket_objects;
        std::mutex _system_lock;

        static void __stdcall apc_fun(ULONG_PTR data);

        void interrupt_unsafe();
    public:
        void bind();

        void wait(Time::time_delta_type_us delay);

        void run_now(Utils::Callback<>::raw_type fn);

        void interrupt();

        void start_socket(std::shared_ptr<SocketEventHandle> socket);

        void close_socket(std::shared_ptr<SocketEventHandle> socket);

        bool extract_socket(std::shared_ptr<SocketEventHandle> socket);

        ~SocketEventEmitterProviderImpl();
    };
}

#endif