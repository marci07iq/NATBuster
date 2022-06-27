#pragma once

#ifdef WIN32

#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#include "network.h"
#include "../utils/copy_protection.h"

namespace NATBuster::Common::Network {

    //
    // Wrap OS sockets into a non-copyable object
    //

    class SocketWrapper : Utils::NonCopyable {
        SOCKET _socket;
    public:
        SocketWrapper(SOCKET socket = INVALID_SOCKET) : _socket(socket) {

        }

        inline bool valid() const {
            return _socket != INVALID_SOCKET;
        }

        inline bool invalid() const {
            return _socket == INVALID_SOCKET;
        }

        inline void close() {
            if (valid()) {
                closesocket(_socket);
                _socket = INVALID_SOCKET;
            }
        }

        inline void set(SOCKET socket) {
            close();
            _socket = socket;
        }

        inline SOCKET get() {
            return _socket;
        }

        ~SocketWrapper() {
            close();
        }
    };

    class NetworkAddress {
        sockaddr_in _address;

        const int _address_length = sizeof(sockaddr_in);

        static_assert(sizeof(sockaddr_in) == sizeof(sockaddr));
    public:
        NetworkAddress();

        NetworkAddress(std::string name, uint16_t port);

        inline sockaddr* get() {
            return (sockaddr*)&_address;
        }

        inline const sockaddr* get() const {
            return (sockaddr*)&_address;
        }

        inline int size() const {
            return _address_length;
        }

        inline uint32_t get_ipv4_int() const {
            return _address.sin_addr.s_addr;
        }

        inline std::string get_ipv4() const {
            char* res = inet_ntoa(_address.sin_addr);

            return std::string(res);
        }

        inline uint16_t get_port() const {
            return _address.sin_port;
        }

        inline bool operator==(const NetworkAddress& rhs) const {
            return memcmp(&_address, &rhs._address, sizeof(sockaddr_in)) == 0;
        }

        inline bool operator!=(const NetworkAddress& rhs) const {
            return memcmp(&_address, &rhs._address, sizeof(sockaddr_in)) != 0;
        }
    };

    //
    // TCP Server OS implementation
    // 

    class TCPS {
        SocketWrapper _listen_socket;
    public:
        TCPS(std::string name, uint16_t port);

        bool valid();
        void close();

        TCPCHandle accept();

        static TCPSHandle find(const std::list<TCPSHandle>& sockets, int64_t timeout);

        ~TCPS();
    };

    //
    // TCP Client OS implementation
    //

    class TCPC {
        SocketWrapper _client_socket;
    public:
        TCPC(std::string name, uint16_t port);
        TCPC(SOCKET client);

        bool valid();
        void close();

        bool send(Packet data);
        Packet read(uint32_t min_len, uint32_t max_len);

        static TCPCHandle find(const std::list<TCPCHandle>& sockets, int64_t timeout);

        ~TCPC();
    };

    //
    // UDP
    //

    class UDP {
        SocketWrapper _socket;
        NetworkAddress _remote_address;
        NetworkAddress _local_address;
    public:
        UDP(std::string remote_name, uint16_t remote_port, uint16_t local_port = 0);
        UDP(NetworkAddress remote_address, NetworkAddress local_address);

        bool valid();
        void close();

        bool send(Packet data);
        Packet readFilter(uint32_t max_len);
        Packet read(uint32_t max_len, NetworkAddress& address);

        static UDPHandle find(const std::list<UDPHandle>& sockets, int64_t timeout);

        ~UDP();
    };
}

#endif