#pragma once

#ifdef WIN32

#include <iostream>
#include <stdio.h>
#include <array>

#include "../os.h"

#include "network_common.h"
#include "../utils/copy_protection.h"
#include "../utils/blob.h"

namespace NATBuster::Common::Network {

    static const int SELECT_MAX = FD_SETSIZE;

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
                //std::cout << "CLOSE SOCKET" << std::endl;
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
        enum Type {
            Unknown,
            IPV4,
            IPV6
        } _type;

        SOCKADDR_STORAGE _address;

        DWORD _address_length = sizeof(_address);
    public:
        NetworkAddress();

        NetworkAddress(const std::string& name, uint16_t port);

        inline const SOCKADDR_STORAGE* get() const {
            return (SOCKADDR_STORAGE*)&_address;
        }

        inline SOCKADDR_STORAGE* getw() {
            return (SOCKADDR_STORAGE*)&_address;
        }

        inline const DWORD size() const {
            return _address_length;
        }

        inline DWORD* sizew() {
            return &_address_length;
        }

        inline std::string get_addr() const {
            if (_address.ss_family == AF_INET) {
                std::array<char, 16> res;
                inet_ntop(AF_INET, &((sockaddr_in*)(&_address))->sin_addr, res.data(), res.size());
                std::string res_str(res.data());
                return res_str;
            }
            if (_address.ss_family == AF_INET6) {
                std::array<char, 46> res;
                inet_ntop(AF_INET6, &((sockaddr_in6*)(&_address))->sin6_addr, res.data(), res.size());
                std::string res_str(res.data());
                return res_str;
            }
            return std::string();
        }

        inline uint16_t get_port() const {
            if (_address.ss_family == AF_INET) {
                return ntohs(((sockaddr_in*)(&_address))->sin_port);
            }
            if (_address.ss_family == AF_INET6) {
                return ntohs(((sockaddr_in6*)(&_address))->sin6_port);
            }
            return 0;
        }

        inline bool operator==(const NetworkAddress& rhs) const {
            if (_address_length != rhs._address_length) return false;
            return memcmp(&_address, &rhs._address, _address_length) == 0;
        }

        inline bool operator!=(const NetworkAddress& rhs) const {
            return !(this->operator==(rhs));
        }
    };

    std::ostream& operator<<(std::ostream& os, const NetworkAddress& addr);

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
        static Utils::PollResponse<MY_HWND> find(std::list<MY_HWND>& sockets, Time::Timeout timeout);
    };

    //
    // TCP Server OS implementation
    // 

    class TCPS : public SocketBase<TCPSHandle> {
    public:
        TCPS(const std::string& name, uint16_t port);

        TCPCHandle accept();

        ~TCPS();
    };

    //
    // TCP Client OS implementation
    //

    class TCPC : public SocketBase<TCPCHandle> {
    private:
        NetworkAddress _remote_addr;

        TCPC(SOCKET client, NetworkAddress addr);
        friend class TCPS;
    public:
        TCPC(const std::string& name, uint16_t port);

        bool send(const Utils::ConstBlobView& data);
        bool read(Utils::BlobView& data, uint32_t min_len, uint32_t max_len);

        //const NetworkAddress& get_local_addr();
        const NetworkAddress& get_remote_addr();

        ~TCPC();
    };

    //
    // UDP
    //

    class UDP : public SocketBase<UDPHandle> {
        NetworkAddress _local_address;
        NetworkAddress _remote_address;
    public:
        UDP(const std::string& remote_name, uint16_t remote_port, uint16_t local_port = 0);
        UDP(NetworkAddress remote_address, NetworkAddress local_address);

        bool send(const Utils::ConstBlobView& data);
        bool readFilter(Utils::BlobView& data, uint32_t max_len = 3000);
        bool read(Utils::BlobView& data, NetworkAddress& address, uint32_t max_len = 3000, bool conn_reset_fatal = true);

        void replaceRemote(NetworkAddress remote_address);

        inline const NetworkAddress& getLocal() const {
            return _local_address;
        }
        inline const NetworkAddress& getRemote() const {
            return _remote_address;
        }

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
    Utils::PollResponse<MY_HWND> SocketBase<MY_HWND>::find(std::list<MY_HWND>& sockets, Time::Timeout timeout) {
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

        for (auto& socket : sockets) {
            SocketWrapper& sw = socket->_socket;

            if (sw.valid()) {
                if (FD_ISSET(socket->_socket.get(), &collection)) {
                    MY_HWND res = socket;
                    return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::OK, std::move(res));
                }
            }
        }

        return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::UnknownError);
    }

}

#endif