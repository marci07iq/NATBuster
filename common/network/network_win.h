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
    class NetworkAddressOSData {
    public:
        SOCKADDR_STORAGE _address;

        DWORD _address_length = sizeof(_address);

        NetworkAddressOSData();
        
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
    };

    std::ostream& operator<<(std::ostream& os, const NetworkAddressOSData& addr);

    //OS Event wrapper
    typedef HANDLE EventOSHandle;

    //RAII move-only wrapper for OS sockets
    class SocketOSData : Utils::NonCopyable {
        SOCKET _socket = INVALID_SOCKET;
    public:
        SocketOSData(SOCKET socket = INVALID_SOCKET);
        SocketOSData(SocketOSData&& other);
        SocketOSData& operator=(SocketOSData&& other);
        inline void set(SOCKET socket);

        inline SOCKET get();

        inline bool is_valid() const;
        inline bool is_invalid() const;

        void set_events(EventOSHandle& hwnd);

        inline void close();
        ~SocketOSData();
    };

    class SocketEventEmitterImpl : Utils::NonCopyable {
        //Only access from thread

        std::vector<EventOSHandle> _socket_events;
        std::vector<std::shared_ptr<SocketEventHandle>> _socket_objects;

        //Access from any thread

        HANDLE _this_thread;
        std::list<Common::Utils::Callback<>> _tasks;
        std::list<std::shared_ptr<SocketEventHandle>> _added_socket_objects;
        std::mutex _system_lock;

        static void __stdcall apc_fun(ULONG_PTR data) {

        }
    public:
        void bind() {
            std::lock_guard _lg(_system_lock);
            DuplicateHandle(
                GetCurrentProcess(),
                GetCurrentThread(),
                GetCurrentProcess(),
                &_this_thread,
                0,
                TRUE,
                DUPLICATE_SAME_ACCESS);
        }

        void wait(Time::time_delta_type_us delay) {
            int64_t delay_ms = (delay + 999) / 1000;
            DWORD timeout = (delay_ms < std::numeric_limits<DWORD>::max()) ? delay_ms : INFINITE;
            if (delay < 0) timeout = INFINITE;

            DWORD ncount = _socket_events.size();

            DWORD res = WSAWaitForMultipleEvents(
                ncount,
                _socket_events.data(),
                FALSE, 10000, TRUE);

            if (res == WAIT_IO_COMPLETION) {
                //APC triggered
            }
            else if (res == WAIT_TIMEOUT) {
                //Timeout triggered
            }
            else if (WAIT_OBJECT_0 <= res && res < (WAIT_OBJECT_0 + ncount)) {
                int index = res - WAIT_OBJECT_0;

                WSANETWORKEVENTS set_events;
                //TODO: Handle errors
                int res2 = WSAEnumNetworkEvents(
                    _socket_objects[index]->_socket->get(),
                    _socket_events[index],
                    &set_events);

                if (set_events.lNetworkEvents & FD_CONNECT) {
                    _socket_objects[index]->_callback_connect();
                }

                if (set_events.lNetworkEvents & FD_READ) {
                    Utils::Blob data = Utils::Blob::factory_empty(4000);
                    WSABUF buffer;
                    buffer.buf = (CHAR*)data.getw();
                    buffer.len = data.size();
                    DWORD received;
                    DWORD flags = 0;
                    int res = WSARecv(
                        _socket_objects[index]->_socket->get(),
                        &buffer,
                        1,
                        &received,
                        &flags,
                        NULL,
                        NULL);

                    //Closed
                    if (received == 0) {
                        //TODO: Prevent double firing
                        _socket_objects[index]->_callback_close();
                    }
                    else {
                        data.resize(received);
                        _socket_objects[index]->_callback_packet(data);
                    }
                }

                if (set_events.lNetworkEvents & FD_ACCEPT) {
                    TCPCHandleU hwnd;

                    //TODO

                    _socket_objects[index]->_callback_accept(std::move(hwnd));
                }

                if (set_events.lNetworkEvents & FD_CLOSE) {
                    _socket_objects[index]->_callback_close();
                }
            }

            //Execute updates
            {
                std::lock_guard _lg(_system_lock);
                //TODO
            }
        }

        void run_now(Common::Utils::Callback<>::raw_type fn) {
            std::lock_guard _lg(_system_lock);
            _tasks.push_back(fn);
            QueueUserAPC(SocketEventEmitterImpl::apc_fun, _this_thread, 0);
        }

        void interrupt() {
            std::lock_guard _lg(_system_lock);
            QueueUserAPC(SocketEventEmitterImpl::apc_fun, _this_thread, 0);
        }

        void add_socket(std::shared_ptr<SocketEventHandle> socket) {
            std::lock_guard _lg(_system_lock);
            _added_socket_objects.push_back(socket);
            QueueUserAPC(SocketEventEmitterImpl::apc_fun, _this_thread, 0);
        }
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