#pragma once

#ifdef WIN32

#define NOMINMAX

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "Ws2_32.lib")

#undef NOMINMAX

#include <array>
#include <stdint.h>
#include <string>

#include "network_common.h"
#include "network.h"
#include "../utils/copy_protection.h"
#include "../utils/blob.h"
#include "../error/codes.h"

namespace NATBuster::Common::Network {

    //Network address OS defined implementation
    //Represent an address (IP and port), IPV4 or IPV6
    class NetworkAddressImpl {
        enum Type {
            Unknown,
            IPV4,
            IPV6
        } _type;

        SOCKADDR_STORAGE _address;

        DWORD _address_length = sizeof(_address);
    public:
        NetworkAddressImpl();
        ErrorCode resolve(const std::string& name, uint16_t port);

        inline const SOCKADDR_STORAGE* get_data() const {
            return (SOCKADDR_STORAGE*)&_address;
        }
        inline SOCKADDR_STORAGE* get_dataw() {
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

        inline bool operator==(const NetworkAddressImpl& rhs) const {
            if (_address_length != rhs._address_length) return false;
            return memcmp(&_address, &rhs._address, _address_length) == 0;
        }
        inline bool operator!=(const NetworkAddressImpl& rhs) const {
            return !(this->operator==(rhs));
        }
    };

    std::ostream& operator<<(std::ostream& os, const NetworkAddressImpl& addr);

    //RAII move-only wrapper for OS sockets
    class SocketImpl : Utils::NonCopyable {
        SOCKET _socket = INVALID_SOCKET;
    public:
        SocketImpl(SOCKET socket = INVALID_SOCKET);
        SocketImpl(SocketImpl&& other);
        SocketImpl& operator=(SocketImpl&& other);
        inline void set(SOCKET socket);

        inline SOCKET get();

        inline bool is_valid() const;
        inline bool is_invalid() const;

        inline void close();
        ~SocketImpl();
    };



    //RAII wrapped WSA Event structure
    class EventImpl {
        WSAEVENT _event = INVALID_HANDLE_VALUE;
    public:
        EventImpl(EventImpl&& other) noexcept {
            _event = other._event;
            other._event = INVALID_HANDLE_VALUE;
        }
        EventImpl& operator=(EventImpl&& other) noexcept {
            _event = other._event;
            other._event = INVALID_HANDLE_VALUE;
        }

        void close() {
            if (_event != INVALID_HANDLE_VALUE) {
                WSACloseEvent(_event);
                _event = INVALID_HANDLE_VALUE;
            }
        }
        ~EventImpl() {
            close();
        }
    };



    class SocketPoolImpl {
    public:
        using 

        struct SocketEvent {
            SocketImpl socket;
            EventImpl evt;


        };

        std::list<SocketEvent> _sockets;

    public:

    };


    /*template <typename MY_HWND>
    class SocketBase {
    protected:
        SocketImpl _socket;

        SocketBase() {

        }
        SocketBase(SOCKET socket) : _socket(socket) {

        }

        template<typename HWND>
        friend class SocketBase;
    public:
        inline bool is_valid() {
            return _socket.is_valid();
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

        if (_socket.is_valid()) {
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

        if (_socket.is_valid()) {
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
            SocketImpl& sw = socket->_socket;

            if (sw.is_valid()) {
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
            SocketImpl& sw = socket->_socket;

            if (sw.is_valid()) {
                if (FD_ISSET(socket->_socket.get(), &collection)) {
                    MY_HWND res = socket;
                    return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::OK, std::move(res));
                }
            }
        }

        return Utils::PollResponse<MY_HWND>(Utils::PollResponseType::UnknownError);
    }*/

}

#endif