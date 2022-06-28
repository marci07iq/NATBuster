#pragma once

#ifdef WIN32

#include <iostream>
#include <stdio.h>

#include "../os.h"

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

    template <typename MY_HWND>
    class SocketBase : public Utils::AbstractBase {
    protected:
        SocketWrapper _socket;

        SocketBase() {

        }
        SocketBase(SOCKET socket) : _socket(socket) {

        }

    public:
        inline bool valid() {
            return _socket.valid();
        }
        inline void close() {
            return _socket.close();
        }

        Utils::EventResponse<Utils::Void> check(Time::Timeout timeout);
        static Utils::EventResponse<MY_HWND> find(const std::list<MY_HWND>& sockets, Time::Timeout timeout);
    };

    //
    // TCP Server OS implementation
    // 

    class TCPS : public SocketBase<TCPSHandle> {
    public:
        TCPS(std::string name, uint16_t port);

        TCPCHandle accept();

        ~TCPS();
    };

    //
    // TCP Client OS implementation
    //

    class TCPC : public SocketBase<TCPCHandle> {
    private:
        TCPC(SOCKET client);
        friend class TCPS;
    public:
        TCPC(std::string name, uint16_t port);

        bool send(Packet data);
        Packet read(uint32_t min_len, uint32_t max_len);

        ~TCPC();
    };

    //
    // UDP
    //

    class UDP : public SocketBase<UDPHandle> {
        NetworkAddress _remote_address;
        NetworkAddress _local_address;
    public:
        UDP(std::string remote_name, uint16_t remote_port, uint16_t local_port = 0);
        UDP(NetworkAddress remote_address, NetworkAddress local_address);

        bool send(Packet data);
        Packet readFilter(uint32_t max_len);
        Packet read(uint32_t max_len, NetworkAddress& address);

        ~UDP();
    };
}

#endif