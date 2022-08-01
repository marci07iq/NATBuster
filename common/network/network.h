#pragma once

#include <iostream>
#include <stdint.h>
#include <string>

#include "../error/codes.h"
#include "../utils/blob.h"
#include "../utils/callbacks.h"
#include "../utils/event.h"

namespace NATBuster::Common::Network {
    //Network address abstract representation
    class NetworkAddressOSData;

    class NetworkAddress {
        std::unique_ptr<NetworkAddressOSData> _impl;
    public:
        enum Type {
            Unknown,
            IPV4,
            IPV6
        };

        NetworkAddress();
        NetworkAddress(NetworkAddress& other);
        NetworkAddress(NetworkAddress&& other);
        NetworkAddress& operator=(NetworkAddress& other);
        NetworkAddress& operator=(NetworkAddress&& other);

        ErrorCode resolve(const std::string& name, uint16_t port);

        inline Type get_type() const;
        inline std::string get_addr() const;
        inline uint16_t get_port() const;
        inline NetworkAddressOSData* get_impl() const;

        inline bool operator==(const NetworkAddress& rhs) const;
        inline bool operator!=(const NetworkAddress& rhs) const;

    };
    std::ostream& operator<<(std::ostream& os, const NetworkAddress& addr);

    //OS RAII socket implementation
    class SocketOSData;
    typedef std::unique_ptr<SocketOSData> SocketOSHandle;
    class SocketEventEmitterImpl;
    class SocketEventEmitter;

    class TCPS;
    class TCPC;
    class UDP;
    typedef Utils::shared_unique_ptr<TCPS> TCPSHandleU;
    typedef Utils::shared_unique_ptr<TCPC> TCPCHandleU;
    typedef Utils::shared_unique_ptr<UDP> UDPHandleU;
    typedef std::shared_ptr<TCPS> TCPSHandleS;
    typedef std::shared_ptr<TCPC> TCPCHandleS;
    typedef std::shared_ptr<UDP> UDPHandleS;


    class SocketBase : Utils::NonCopyable {
    protected:
        SocketOSHandle _socket;

        friend class SocketEventEmitter;
        friend class SocketEventEmitterImpl;

    public:
        SocketBase() {

        }
        SocketBase(SocketOSHandle&& socket) : _socket(std::move(socket)) {

        }
        void set(SocketOSHandle&& socket) {
            _socket = std::move(socket);
        }

        //Extract the underlying socket
        SocketOSHandle extract() {
            return std::move(_socket);
        }

        inline bool is_valid();

        inline bool is_invalid();

        inline void close();
    };


    class SocketEventHandle : private SocketBase {
    public:
        //Called from client side TCPC, when connected
        using ConnectCallback = Utils::Callback<>;
        //Called from TCPS and UDP when bound
        //using BindCallback = Utils::Callback<>;
        //Called from TCPS when a new connection arrived
        using AcceptCallback = Utils::Callback<TCPCHandleU&&>;
        //Called from TCPC and UDP when packet received
        using PacketCallback = Utils::Callback<const Utils::ConstBlobView&>;
        //Called from UDP when any packet is received
        using UnfilteredPacketCallback = Utils::Callback<const Utils::ConstBlobView&, NetworkAddress&>;
        //Called from any socket, when an error occurs
        using ErrorCallback = Utils::Callback<ErrorCode>;
        //Called from any socket, when closed by remote or local.
        using CloseCallback = Utils::Callback<>;

    private:

        ConnectCallback _callback_connect;
        //BindCallback _callback_bind;
        AcceptCallback _callback_accept;
        PacketCallback _callback_packet;
        UnfilteredPacketCallback _callback_unfiltered_packet;
        ErrorCallback _callback_error;
        CloseCallback _callback_close;

        friend class SocketEventEmitter;
        friend class SocketEventEmitterImpl;

    protected:
        using SocketBase::_socket;

        SocketEventHandle();
        SocketEventHandle(SocketOSHandle&& socket);

        std::shared_ptr<SocketEventEmitter> _base;

        inline void set_callback_connect(ConnectCallback::raw_type callback_connect) {
            _callback_connect = callback_connect;
        }
        /*inline void set_callback_bind(BindCallback::raw_type callback_bind) {
            _callback_bind = callback_bind;
        }*/
        inline void set_callback_accept(AcceptCallback::raw_type callback_accept) {
            _callback_accept = callback_accept;
        }
        inline void set_callback_packet(PacketCallback::raw_type callback_packet) {
            _callback_packet = callback_packet;
        }
        inline void set_callback_unfiltered_packet(UnfilteredPacketCallback::raw_type callback_unfiltered_packet) {
            _callback_unfiltered_packet = callback_unfiltered_packet;
        }
    public:
        using SocketBase::is_valid;
        using SocketBase::is_invalid;

        inline void set_callback_error(ErrorCallback::raw_type callback_error) {
            _callback_error = callback_error;
        }
        inline void set_callback_close(CloseCallback::raw_type callback_close) {
            _callback_close = callback_close;
        }
    };

    class TCPS : public SocketEventHandle, public Utils::SharedOnly<TCPS> {
        std::list<TCPSHandleU>::iterator _self;

        TCPS();

        ErrorCode bind(const std::string& name, uint16_t port);

        friend class SocketEventEmitter;
        friend class SocketEventEmitterImpl;
    public:
        using SocketEventHandle::set_callback_accept;

        std::pair<Utils::shared_unique_ptr<TCPS>, ErrorCode>
            create(const std::string& name, uint16_t port) {

            Utils::shared_unique_ptr<TCPS> res = Utils::shared_unique_ptr<TCPS>();
            ErrorCode code = res->bind(name, port);

            return std::make_pair<Utils::shared_unique_ptr<TCPS>, ErrorCode>
                (std::move(res), code);
        }

        void drop();
    };

    class TCPC : public SocketEventHandle, public Utils::SharedOnly<TCPC> {
        std::list<TCPCHandleU>::iterator _self;

        NetworkAddress _remote_address;

        TCPC();

        TCPC(SocketOSHandle&& socket, NetworkAddress&& remote_address);

        ErrorCode connect(const std::string& name, uint16_t port);

        friend class SocketEventEmitter;
        friend class SocketEventEmitterImpl;
    public:
        using SocketEventHandle::set_callback_connect;
        using SocketEventHandle::set_callback_packet;

        std::pair<Utils::shared_unique_ptr<TCPC>, ErrorCode>
            create(const std::string& name, uint16_t port) {

            Utils::shared_unique_ptr<TCPC> res = Utils::shared_unique_ptr<TCPC>();
            ErrorCode code = res->connect(name, port);

            return std::make_pair<Utils::shared_unique_ptr<TCPC>, ErrorCode>
                (std::move(res), code);
        }

        NetworkAddress& get_remote();

        void send(Utils::ConstBlobView& data);

        void drop();
    };

    class UDP : public SocketEventHandle, public Utils::SharedOnly<UDP> {
        std::list<UDPHandleU>::iterator _self;

        NetworkAddress _local_address;
        NetworkAddress _remote_address;

        //Bind local socket
        ErrorCode bind(NetworkAddress&& local);
        ErrorCode bind(const std::string& name, uint16_t port) {
            NetworkAddress address;
            ErrorCode code = address.resolve(name, port);
            if (code != ErrorCode::OK) return code;
            return bind(address);
        }

        UDP();

        friend class SocketEventEmitter;
        friend class SocketEventEmitterImpl;
    public:
        using SocketEventHandle::set_callback_packet;
        using SocketEventHandle::set_callback_unfiltered_packet;


        std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
            create(NetworkAddress&& local) {

            Utils::shared_unique_ptr<UDP> res = Utils::shared_unique_ptr<UDP>();
            ErrorCode code = res->bind(std::move(local));

            return std::make_pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
                (std::move(res), code);
        }
        std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
            create(const std::string& local_name, uint16_t local_port) {

            Utils::shared_unique_ptr<UDP> res = Utils::shared_unique_ptr<UDP>();
            ErrorCode code = res->bind(name, port);

            return std::make_pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
                (std::move(res), code);
        }

        //Set remote socket
        ErrorCode set_remote(NetworkAddress&& remote) {
            _remote_address = std::move(remote);
        }
        ErrorCode set_remote(const std::string& name, uint16_t port) {
            NetworkAddress address;
            ErrorCode code = address.resolve(name, port);
            if (code != ErrorCode::OK) return code;
            return set_remote(std::move(address));
        }

        void send(Utils::ConstBlobView& data);

        void drop();
    };

    //EventEmitterProvider with socket watching capability
    class SocketEventEmitter : public Utils::EventEmitterProvider, public Utils::SharedOnly<SocketEventEmitter> {
    private:
        std::unique_ptr<SocketEventEmitterImpl> _impl;

        std::list<TCPSHandleU> _sockets_tcps;
        std::list<TCPCHandleU> _sockets_tcpc;
        std::list<UDPHandleU> _sockets_udp;

        std::mutex _sockets_lock;

        virtual ~SocketEventEmitter();
    public:
        void bind();

        void wait(Time::time_delta_type_us delay);

        void run_now(Common::Utils::Callback<>::raw_type fn);

        void interrupt();

        void add_socket(TCPSHandleU hwnd);
        void add_socket(TCPCHandleU hwnd);
        void add_socket(UDPHandleU hwnd);

        void close_socket(TCPSHandleS hwnd);
        void close_socket(TCPCHandleS hwnd);
        void close_socket(UDPHandleS hwnd);
    };
}