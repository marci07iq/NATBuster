#pragma once

#ifdef WIN32

#include <iostream>
#include <stdio.h>

#include "../os.h"

#include "network_common.h"
#include "../utils/copy_protection.h"
#include "../utils/blob.h"

namespace NATBuster::Common::Network {

    void NetworkError(NetworkErrorCodes internal_id, int os_id);

    //
    // Wrap OS sockets into a non-copyable object
    //

    class SocketWrapper : Utils::NonCopyable {
        SOCKET _socket;
    public:
        SocketWrapper(SOCKET socket = INVALID_SOCKET) : _socket(socket) {

        }

        SocketWrapper(SocketWrapper&& other) noexcept {
            _socket = other._socket;
            other._socket = INVALID_SOCKET;
        }

        SocketWrapper& operator=(SocketWrapper&& other) noexcept {
            close();
            _socket = other._socket;
            other._socket = INVALID_SOCKET;
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

        NetworkAddress(const std::string& name, uint16_t port);

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
    class SocketBase {
    protected:
        SocketWrapper _socket;

        SocketBase() {

        }
        SocketBase(SOCKET socket) : _socket(socket) {

        }

        template<typename HWND>
        friend class SocketBase;
    public:
        inline bool valid() {
            return _socket.valid();
        }
        inline void close() {
            return _socket.close();
        }

        Utils::PollResponse<Utils::Void> check(Time::Timeout timeout);
        static Utils::PollResponse<MY_HWND> find(const std::list<MY_HWND>& sockets, Time::Timeout timeout);
    };

    //
    // TCP Server OS implementation
    // 

    class TCPS : public SocketBase<TCPSHandle> {
    public:
        TCPS(const std::string& name, uint16_t port);

        TCPCHandle accept(NetworkAddress& src);

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
        TCPC(const std::string& name, uint16_t port);

        bool send(const Utils::ConstBlobView& data);
        bool read(Utils::BlobView& data, uint32_t min_len, uint32_t max_len);

        ~TCPC();
    };

    //
    // UDP
    //

    class UDP : public SocketBase<UDPHandle> {
        NetworkAddress _remote_address;
        NetworkAddress _local_address;
    public:
        UDP(const std::string& remote_name, uint16_t remote_port, uint16_t local_port = 0);
        UDP(NetworkAddress remote_address, NetworkAddress local_address);

        bool send(const Utils::ConstBlobView& data);
        bool readFilter(Utils::BlobView& data, uint32_t max_len = 3000);
        bool read(Utils::BlobView& data, NetworkAddress& address, uint32_t max_len = 3000);

        ~UDP();
    };


    template <typename MY_HWND>
    Utils::PollResponse<Utils::Void> SocketBase<MY_HWND>::check(Time::Timeout timeout) {
        FD_SET collection;

        FD_ZERO(&collection);

        if (_socket.valid()) {
            FD_SET(_socket.get(), &collection);
        }
        else {
            return Utils::PollResponse<Utils::Void>(Utils::PollResponseType::ClosedError);
        }

        int count = select(0, &collection, nullptr, nullptr, timeout.to_native());

        if (count == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeSelectRead, WSAGetLastError());
            return Utils::PollResponse<Utils::Void>(Utils::PollResponseType::UnknownError);
        }

        if (count == 0) {
            return Utils::PollResponse<Utils::Void>(Utils::PollResponseType::Timeout);
        }

        if (_socket.valid()) {
            if (FD_ISSET(_socket.get(), &collection)) {
                return Utils::PollResponse<Utils::Void>(Utils::PollResponseType::OK);
            }
        }

        return Utils::PollResponse<Utils::Void>(Utils::PollResponseType::UnknownError);
    }

    template <typename MY_HWND>
    Utils::PollResponse<MY_HWND> SocketBase<MY_HWND>::find(const std::list<MY_HWND>& sockets, Time::Timeout timeout) {
        FD_SET collection;

        FD_ZERO(&collection);

        int entries = 0;
        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_socket;

            if (sw.valid()) {
                FD_SET(sw.get(), &collection);
                ++entries;
            }
        }
        //No sockets
        if (entries == 0) {
            return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::ClosedError);
        }

        int count = select(0, &collection, nullptr, nullptr, timeout.to_native());

        if (count == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeSelectRead, WSAGetLastError());
            return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::UnknownError);
        }

        if (count == 0) {
            return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::Timeout);
        }

        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_socket;

            if (sw.valid()) {
                if (FD_ISSET(socket->_socket.get(), &collection)) {
                    return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::OK);
                }
            }
        }

        return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::UnknownError);
    }

}

#endif