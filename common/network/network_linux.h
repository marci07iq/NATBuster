#pragma once

#ifdef __linux__

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>

#include <array>
#include <stdint.h>
#include <string>

#include "network.h"

namespace NATBuster::Network {

    typedef int SOCKET;
    static const SOCKET INVALID_SOCKET = -1;

    struct EventHandleOSData : public pollfd {
    };
    static_assert(sizeof(EventHandleOSData) == sizeof(pollfd));

    struct AddrInfoOSData : public addrinfo {
        
    };
    static_assert(sizeof(AddrInfoOSData) == sizeof(addrinfo));


    //Network address OS defined implementation
    //Represent an address (IP and port), IPV4 or IPV6
    class NetworkAddressOSData {
    public:
        sockaddr_storage _address;

        int _address_length = sizeof(_address);

        NetworkAddressOSData();
        NetworkAddressOSData(NetworkAddressOSData& other);
        NetworkAddressOSData& operator=(NetworkAddressOSData& other);

        inline const sockaddr_storage* get_data() const {
            return (sockaddr_storage*)&_address;
        }
        inline sockaddr_storage* get_dataw() {
            return (sockaddr_storage*)&_address;
        }

        inline const int size() const {
            return _address_length;
        }
        inline int* sizew() {
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

        void set_events(pollfd& hwnd);

        inline void close();
        ~SocketOSData();
    };

    class SocketEventEmitterProviderImpl : Utils::NonCopyable {
        //Variables consumed by the thread

        std::vector<pollfd> _socket_events;
        std::vector<std::shared_ptr<SocketEventHandle>> _socket_objects;
        std::mutex _sockets_lock;

        //Fast to access variables to send data to thread

        int _waker_fd = -1;
        std::list<Utils::Callback<>> _tasks;
        std::list<std::shared_ptr<SocketEventHandle>> _added_socket_objects;
        std::list<std::shared_ptr<SocketEventHandle>> _closed_socket_objects;
        std::mutex _system_lock;

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