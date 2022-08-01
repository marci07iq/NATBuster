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

        ErrorCode resolve(const std::string& name, uint16_t port);

        inline Type get_type() const;
        inline std::string get_addr() const;
        inline uint16_t get_port() const;
        inline NetworkAddressOSData* get_impl() const;

        inline bool operator==(const NetworkAddress& rhs) const;
        inline bool operator!=(const NetworkAddress& rhs) const;

    };
    std::ostream& operator<<(std::ostream& os, const NetworkAddressOSData& addr);

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
        SocketOSHandle _socket;

        friend class SocketEventEmitter;
        friend class SocketEventEmitterImpl;
    public:
        SocketBase() {

        }
        SocketBase(SocketOSHandle socket) : _socket(std::move(socket)) {

        }

        //Set the socket
        void set(SocketOSHandle socket);
        //Extract the underlying socket
        SocketOSHandle extract() {
            return std::move(_socket);
        }

        inline bool is_valid();

        inline void close();
    };


    class SocketEventHandle : public SocketBase {
    public:
        //Called from client side TCPC, when connected
        using ConnectCallback = Utils::Callback<>;
        //Called from TCPS and UDP when bound
        //using BindCallback = Utils::Callback<>;
        //Called from TCPS when a new connection arrived
        using AcceptCallback = Utils::Callback<TCPCHandleU&&>;
        //Called from TCPC and UDP when packet received
        using PacketCallback = Utils::Callback<const Utils::ConstBlobView&>;
        //Called from any socket, when an error occurs
        using ErrorCallback = Utils::Callback<ErrorCode>;
        //Called from any socket, when closed by remote or local.
        using CloseCallback = Utils::Callback<>;

    private:
        std::shared_ptr<SocketEventEmitter> _base;

        ConnectCallback _callback_connect;
        //BindCallback _callback_bind;
        AcceptCallback _callback_accept;
        PacketCallback _callback_packet;
        ErrorCallback _callback_error;
        CloseCallback _callback_close;

        friend class SocketEventEmitter;
        friend class SocketEventEmitterImpl;
    public:
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
        inline void set_callback_error(ErrorCallback::raw_type callback_error) {
            _callback_error = callback_error;
        }
        inline void set_callback_close(CloseCallback::raw_type callback_close) {
            _callback_close = callback_close;
        }
    };

    class TCPS : public SocketEventHandle {
        std::list<TCPSHandleU>::iterator _self;

        TCPCHandleU accept();

        friend class SocketEventEmitter;
    public:
        void bind(const std::string& name, uint16_t port);

        void close();
    };

    class TCPC : public SocketEventHandle {
        std::list<TCPCHandleU>::iterator _self;

        NetworkAddress _remote_address;

        bool recv(Utils::Blob& dst);

        friend class SocketEventEmitter;
    public:
        void connect(const std::string& name, uint16_t port);

        NetworkAddress& get_remote();

        void send(Utils::ConstBlobView& data);

        void close();
    };

    class UDP {
        std::list<UDPHandleU>::iterator _self;

        NetworkAddress _local_address;
        NetworkAddress _remote_address;

        bool recv(Utils::Blob& dst, NetworkAddress& src);

        friend class SocketEventEmitter;
    public:
        void send(Utils::ConstBlobView& data);

        void close();
    };

    //EventEmitterProvider with socket watching capability
    class SocketEventEmitter : public Utils::EventEmitterProvider, public Utils::SharedOnly<SocketEventEmitter> {
    private:
        std::unique_ptr<SocketEventEmitterImpl> _impl;

        std::list<TCPSHandleU> _sockets_tcps;
        std::list<TCPCHandleU> _sockets_tcpc;
        std::list<UDPHandleU> _sockets_udp;

        ~SocketEventEmitter();
    public:
        void bind();

        void wait(Time::time_delta_type_us delay);

        void run_now(Common::Utils::Callback<>::raw_type fn);

        void interrupt();

        void add_socket(TCPSHandleU);
        void add_socket(TCPCHandleU);
        void add_socket(UDPHandleU);
    };
}