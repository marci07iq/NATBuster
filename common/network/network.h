#pragma once

#include <iostream>
#include <stdint.h>
#include <string>

#include "../error/codes.h"
#include "../utils/blob.h"
#include "../utils/callbacks.h"
#include "../utils/event.h"

namespace NATBuster::Network {
    struct EventHandleOSData;
    //Network connextion address abstract rep
    struct AddrInfoOSData;

    class AddrInfoHwnd;
    //Network address abstract representation
    class NetworkAddressOSData;
    //OS RAII socket implementation
    class SocketOSData;

    class SocketEventEmitterProviderImpl;
    class SocketEventEmitterProvider;

    class TCPS;
    typedef Utils::shared_unique_ptr<TCPS> TCPSHandleU;
    typedef std::shared_ptr<TCPS> TCPSHandleS;
    class TCPC;
    typedef Utils::shared_unique_ptr<TCPC> TCPCHandleU;
    typedef std::shared_ptr<TCPC> TCPCHandleS;
    class UDP;
    typedef Utils::shared_unique_ptr<UDP> UDPHandleU;
    typedef std::shared_ptr<UDP> UDPHandleS;

    class NetworkAddress {
        NetworkAddressOSData* _impl = nullptr;

        friend class TCPC;
    public:
        enum Type {
            Unknown,
            IPV4,
            IPV6
        };

        NetworkAddress();
        NetworkAddress(const NetworkAddress& other);
        NetworkAddress(NetworkAddress&& other) noexcept;
        NetworkAddress& operator=(const NetworkAddress& other);
        NetworkAddress& operator=(NetworkAddress&& other) noexcept;

        ErrorCode resolve(const std::string& name, uint16_t port);

        Type get_type() const;
        std::string get_addr() const;
        uint16_t get_port() const;
        std::string get_full_addr() const;
        NetworkAddressOSData* get_impl() const;

        static ErrorCode split_network_string(const std::string& full_name, std::string& name, uint16_t& port);

        inline bool operator==(const NetworkAddress& rhs) const;
        inline bool operator!=(const NetworkAddress& rhs) const;

        ~NetworkAddress();
    };
    std::ostream& operator<<(std::ostream& os, const NetworkAddress& addr);


    class SocketBase : Utils::NonCopyable {
    protected:
        SocketOSData* _socket;

        friend class SocketEventEmitterProvider;
        friend class SocketEventEmitterProviderImpl;

        void close();
    public:
        SocketBase();
        SocketBase(SocketOSData* socket) noexcept;
        SocketBase(SocketBase&& other) noexcept;
        SocketBase& operator=(SocketBase&& other) noexcept;

        //Extract the underlying socket
        SocketOSData* extract();

        bool is_valid();

        bool is_invalid();

        ~SocketBase();
    };






    class SocketEventHandle : private SocketBase {
    public:
        //Called from any socket, when added to the emitter
        using StartCallback = Utils::Callback<>;
        //Called from client side TCPC, when connected
        using ConnectCallback = Utils::Callback<>;
        //Called from TCPS when a new connection arrived
        using AcceptCallback = Utils::Callback<TCPCHandleU&&>;
        //Called from TCPC and UDP when packet received
        using PacketCallback = Utils::Callback<const Utils::ConstBlobView&>;
        //Called from UDP when any packet is received
        using UnfilteredPacketCallback = Utils::Callback<const Utils::ConstBlobView&, const NetworkAddress&>;
        //Called from any socket, when an error occurs
        using ErrorCallback = Utils::Callback<ErrorCode>;
        //Called from any socket, when closed by remote or local.
        using CloseCallback = Utils::Callback<>;

        const enum class Type {
            Unknown,
            TCPS,
            TCPC,
            UDP,
        } _type;

    protected:

        StartCallback _callback_start;
        ConnectCallback _callback_connect;
        AcceptCallback _callback_accept;
        PacketCallback _callback_packet;
        UnfilteredPacketCallback _callback_unfiltered_packet;
        ErrorCallback _callback_error;
        CloseCallback _callback_close;

        friend class SocketEventEmitterProvider;
        friend class SocketEventEmitterProviderImpl;

    protected:
        NetworkAddress _remote_address;

        using SocketBase::_socket;

        SocketEventHandle(Type type);
        SocketEventHandle(Type type, SocketOSData* socket);

        std::shared_ptr<SocketEventEmitterProvider> _base;

        uint16_t _recvbuf_len = 4000;


        inline void set_callback_connect(ConnectCallback::raw_type callback_connect) {
            _callback_connect = callback_connect;
        }
        inline void set_callback_accept(AcceptCallback::raw_type callback_accept) {
            _callback_accept = callback_accept;
        }
        inline void set_callback_packet(PacketCallback::raw_type callback_packet) {
            _callback_packet = callback_packet;
        }
        inline void set_callback_unfiltered_packet(UnfilteredPacketCallback::raw_type callback_unfiltered_packet) {
            _callback_unfiltered_packet = callback_unfiltered_packet;
        }

        virtual ErrorCode next_connect_attempt(EventHandleOSData* event_binder) = 0;
    public:
        using SocketBase::is_valid;
        using SocketBase::is_invalid;

        inline std::shared_ptr<SocketEventEmitterProvider> get_base() {
            return _base;
        }

        NetworkAddress& get_remote();

        inline void set_recvbuf_len(uint16_t recvbuf_len) {
            _recvbuf_len = recvbuf_len;
        }

        inline void set_callback_start(StartCallback::raw_type callback_start) {
            _callback_start = callback_start;
        }
        inline void set_callback_error(ErrorCallback::raw_type callback_error) {
            _callback_error = callback_error;
        }
        inline void set_callback_close(CloseCallback::raw_type callback_close) {
            _callback_close = callback_close;
        }

        //Start emitting events
        virtual void start() = 0;
        //Stop emitting events, and close socket
        virtual bool close() = 0;
    };




    class TCPS : public SocketEventHandle, public Utils::SharedOnly<TCPS> {
        friend class Utils::SharedOnly<TCPS>;
        std::list<TCPSHandleU>::iterator _self;

        TCPS();

        ErrorCode bind(const std::string& name, uint16_t port);

        friend class SocketEventEmitterProvider;
        friend class SocketEventEmitterProviderImpl;

        ErrorCode next_connect_attempt(EventHandleOSData* event_binder) override {
            (void)event_binder;
            assert(false);
            return ErrorCode::GENERIC_FATAL_INVALID_VF;
        }
    public:
        using SocketEventHandle::set_callback_accept;

        static std::pair<Utils::shared_unique_ptr<TCPS>, ErrorCode>
            create_bind(const std::string& name, uint16_t port);

        void start() override;
        bool close() override;
    };

    class TCPC : public SocketEventHandle, public Utils::SharedOnly<TCPC> {
        friend class Utils::SharedOnly<TCPC>;
        std::list<TCPCHandleU>::iterator _self;

        AddrInfoOSData* _addrs = nullptr;
        AddrInfoOSData* _addr_current = nullptr;

        TCPC();

        TCPC(SocketOSData* socket, NetworkAddress&& remote_address);


        friend class SocketEventEmitterProvider;
        friend class SocketEventEmitterProviderImpl;
    public:
        using SocketEventHandle::set_callback_connect;
        using SocketEventHandle::set_callback_packet;

        ErrorCode resolve(const std::string& name, uint16_t port);

        static std::pair<Utils::shared_unique_ptr<TCPC>, ErrorCode>
            create_resolve(const std::string& name, uint16_t port);

        void send(const Utils::ConstBlobView& data);

        void start() override;
        bool close() override;

        ErrorCode next_connect_attempt(EventHandleOSData* event_binder);

        ~TCPC();
    };

    class UDP : public SocketEventHandle, public Utils::SharedOnly<UDP> {
        friend class Utils::SharedOnly<UDP>;
        std::list<UDPHandleU>::iterator _self;

        NetworkAddress _local_address;

        //Bind local socket
        ErrorCode bind(NetworkAddress&& local);
        ErrorCode bind(const std::string& name, uint16_t port);

        UDP();

        friend class SocketEventEmitterProvider;
        friend class SocketEventEmitterProviderImpl;

        ErrorCode next_connect_attempt(EventHandleOSData* event_binder) override {
            (void)event_binder;
            assert(false);
            return ErrorCode::GENERIC_FATAL_INVALID_VF;
        }
    public:
        using SocketEventHandle::set_callback_packet;
        using SocketEventHandle::set_callback_unfiltered_packet;


        static std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
            create_bind(NetworkAddress&& local);
        static std::pair<Utils::shared_unique_ptr<UDP>, ErrorCode>
            create_bind(const std::string& local_name, uint16_t local_port);

        //Set remote socket
        void set_remote(const NetworkAddress& remote);
        void set_remote(NetworkAddress&& remote);
        ErrorCode set_remote(const std::string& name, uint16_t port);

        inline const NetworkAddress& get_local() {
            return _local_address;
        }

        void send(const Utils::ConstBlobView& data);
        void sendto(const Utils::ConstBlobView& data, const NetworkAddress& remote_address);

        void start() override;
        bool close() override;
    };

    class UDPMultiplexedRoute;
    class UDPMultiplexed : public Utils::SharedOnly<UDPMultiplexed> {
        friend class Utils::SharedOnly<UDPMultiplexed>;

        std::list<std::weak_ptr<UDPMultiplexedRoute>> _routes;
        std::mutex _routes_lock;

        std::shared_ptr<UDP> _socket;

        SocketEventHandle::StartCallback _callback_start;
        SocketEventHandle::PacketCallback _callback_packet;
        SocketEventHandle::UnfilteredPacketCallback _callback_unfiltered_packet;
        SocketEventHandle::ErrorCallback _callback_error;
        SocketEventHandle::CloseCallback _callback_close;

        friend class UDPMultiplexedRoute;

        UDPMultiplexed(std::shared_ptr<UDP> socket) : _socket(socket) {

        }

    public:
        void on_start();
        void on_packet(const Utils::ConstBlobView& data);
        void on_unfiltered_packet(const Utils::ConstBlobView& data, const NetworkAddress& remote_address);
        void on_error(ErrorCode);
        void on_close();

        //Set remote socket
        Utils::shared_unique_ptr<UDPMultiplexedRoute> add_remote(const NetworkAddress& remote);
        Utils::shared_unique_ptr<UDPMultiplexedRoute> add_remote(NetworkAddress&& remote);
        Utils::shared_unique_ptr<UDPMultiplexedRoute> add_remote(const std::string& name, uint16_t port);

        inline void set_callback_start(SocketEventHandle::StartCallback::raw_type callback_start) {
            _callback_start = callback_start;
        }
        inline void set_callback_packet(SocketEventHandle::PacketCallback::raw_type callback_packet) {
            _callback_packet = callback_packet;
        }
        inline void set_callback_unfiltered_packet(SocketEventHandle::UnfilteredPacketCallback::raw_type callback_unfiltered_packet) {
            _callback_unfiltered_packet = callback_unfiltered_packet;
        }
        inline void set_callback_error(SocketEventHandle::ErrorCallback::raw_type callback_error) {
            _callback_error = callback_error;
        }
        inline void set_callback_close(SocketEventHandle::CloseCallback::raw_type callback_close) {
            _callback_close = callback_close;
        }

        void start();
        bool close();
    };

    class UDPMultiplexedRoute : public Utils::SharedOnly<UDPMultiplexedRoute> {
        friend class Utils::SharedOnly<UDPMultiplexedRoute>;

        NetworkAddress _remote_address;
        std::shared_ptr<UDPMultiplexed> _multiplexer;

        SocketEventHandle::StartCallback _callback_start;
        SocketEventHandle::PacketCallback _callback_packet;
        SocketEventHandle::UnfilteredPacketCallback _callback_unfiltered_packet;
        SocketEventHandle::ErrorCallback _callback_error;
        SocketEventHandle::CloseCallback _callback_close;

        UDPMultiplexedRoute(std::shared_ptr<UDPMultiplexed> multiplexer, const NetworkAddress& remote_address) :
            _multiplexer(multiplexer),
            _remote_address(remote_address) {

        }
        UDPMultiplexedRoute(std::shared_ptr<UDPMultiplexed> multiplexer, NetworkAddress&& remote_address) :
        _multiplexer(multiplexer),
            _remote_address(std::move(remote_address)) {

        }

        friend class UDPMultiplexed;
    public:
        inline const std::shared_ptr<UDP> get_socket() {
            return _multiplexer->_socket;
        }

        inline void set_callback_start(SocketEventHandle::StartCallback::raw_type callback_start) {
            _callback_start = callback_start;
        }
        inline void set_callback_packet(SocketEventHandle::PacketCallback::raw_type callback_packet) {
            _callback_packet = callback_packet;
        }
        inline void set_callback_unfiltered_packet(SocketEventHandle::UnfilteredPacketCallback::raw_type callback_unfiltered_packet) {
            _callback_unfiltered_packet = callback_unfiltered_packet;
        }
        inline void set_callback_error(SocketEventHandle::ErrorCallback::raw_type callback_error) {
            _callback_error = callback_error;
        }
        inline void set_callback_close(SocketEventHandle::CloseCallback::raw_type callback_close) {
            _callback_close = callback_close;
        }

        void send(const Utils::ConstBlobView& data);

        void start();
        bool close();
    };

    //EventEmitterProvider with socket watching capability
    class SocketEventEmitterProvider : public Utils::EventEmitterProvider, public Utils::SharedOnly<SocketEventEmitterProvider> {
        friend class Utils::SharedOnly<SocketEventEmitterProvider>;
    private:
        SocketEventEmitterProviderImpl* _impl = nullptr;

        std::list<TCPSHandleU> _sockets_tcps;
        std::list<TCPCHandleU> _sockets_tcpc;
        std::list<UDPHandleU> _sockets_udp;

        std::mutex _sockets_lock;
    public:
        //Total sockets that can be opened by the process
        static const int MAX_SOCKETS_TOTAL;
        //Total sockets that can be given to one instance
        static const int MAX_SOCKETS_INST;

        SocketEventEmitterProvider();

        void bind();

        void wait(Time::time_delta_type_us delay);

        void run_now(Utils::Callback<>::raw_type fn);

        void interrupt();

        //Associate a socket with this object

        void associate_socket(TCPSHandleU&& hwnd);
        void associate_socket(TCPCHandleU&& hwnd);
        void associate_socket(UDPHandleU&& hwnd);

        //Start emitting events for the socket
        //Returns false if socket wasnt associated with this object
        bool start_socket(std::shared_ptr<SocketEventHandle> hwnd);

        //Close socket
        //Returns false if socket wasn't assocaited

        virtual ~SocketEventEmitterProvider();

        //Only call when not waiting.

        TCPSHandleU extract_socket(TCPSHandleS hwnd);
        TCPCHandleU extract_socket(TCPCHandleS hwnd);
        UDPHandleU extract_socket(UDPHandleS hwnd);

        size_t count();

    private:
        bool close_socket(TCPSHandleS hwnd);
        bool close_socket(TCPCHandleS hwnd);
        bool close_socket(UDPHandleS hwnd);

        friend class TCPS;
        friend class TCPC;
        friend class UDP;
    };
}