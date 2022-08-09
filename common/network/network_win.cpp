#ifdef WIN32

#include "network_win.h"
#include "network.h"

namespace NATBuster::Network {
    //WSA wrapper

    namespace WSA {
        //Class to hold the WSA instance
        class WSAWrapper : Utils::NonCopyable {
            WSADATA _wsa_data;

            WSAWrapper(uint8_t major = 2, uint8_t minor = 2) {
                std::cout << "WSA INIT" << std::endl;
                //Create an instance of winsock
                int iResult = WSAStartup(MAKEWORD(major, minor), &_wsa_data);
                if (iResult != 0) {
                    throw 1;
                }
            }

            static std::shared_ptr<WSAWrapper> get_instance() {
                if (!wsa_instance) {
                    wsa_instance.reset(new WSAWrapper());
                }
                return wsa_instance;
            }

            static std::shared_ptr<WSAWrapper> wsa_instance;

        public:
            ~WSAWrapper() {
                std::cout << "WSA CLEAN" << std::endl;
                WSACleanup();
            }

            WSADATA get() const {
                return _wsa_data;
            }
        };

        std::shared_ptr<WSAWrapper> WSAWrapper::wsa_instance = WSAWrapper::get_instance();
    }

    //NetworkAddressImpl specific

    NetworkAddressOSData::NetworkAddressOSData() {
        _address_length = sizeof(SOCKADDR_STORAGE);
        ZeroMemory(&_address, _address_length);
    }
    NetworkAddressOSData::NetworkAddressOSData(NetworkAddressOSData& other) {
        _address = other._address;
        _address_length = other._address_length;

    }
    NetworkAddressOSData& NetworkAddressOSData::operator=(NetworkAddressOSData& other) {
        _address = other._address;
        _address_length = other._address_length;
        return *this;
    }

    //NetworkAddress functions implemented

    NetworkAddress::NetworkAddress() : _impl(new NetworkAddressOSData()) {
    }
    NetworkAddress::NetworkAddress(NetworkAddress& other) : _impl(new NetworkAddressOSData(*other._impl)) {
    }
    NetworkAddress::NetworkAddress(NetworkAddress&& other) noexcept : _impl(other._impl) {
        other._impl = new NetworkAddressOSData();
    }
    NetworkAddress& NetworkAddress::operator=(NetworkAddress& other) {
        _impl->operator=(*other._impl);
        return *this;
    }
    NetworkAddress& NetworkAddress::operator=(NetworkAddress&& other) noexcept {
        _impl = other._impl;
        other._impl = new NetworkAddressOSData();
        return *this;
    }

    ErrorCode NetworkAddress::resolve(const std::string& name, uint16_t port) {
        ZeroMemory(&_impl->_address, sizeof(SOCKADDR_STORAGE));

        if (name.length()) {
            //Resolve host name to IP
            hostent* server_ip = gethostbyname(name.c_str());

            if (server_ip == nullptr) {
                return ErrorCode::NETWORK_ERROR_RESOLVE_HOSTNAME;
            }

            if ((server_ip->h_addrtype != AF_INET) || (server_ip->h_addr_list[0] == 0)) {
                return ErrorCode::NETWORK_ERROR_RESOLVE_HOSTNAME;
            }

            ((sockaddr_in*)&_impl->_address)->sin_addr.s_addr = *(u_long*)server_ip->h_addr_list[0];
        }
        else {
            //Any address
            ((sockaddr_in*)&_impl->_address)->sin_addr.s_addr = INADDR_ANY;
        }

        //Address
        _impl->_address.ss_family = AF_INET;
        ((sockaddr_in*)&_impl->_address)->sin_family = AF_INET;
        ((sockaddr_in*)&_impl->_address)->sin_port = htons(port);

        return ErrorCode::OK;
    }

    NetworkAddress::Type NetworkAddress::get_type() const {
        if (_impl->_address.ss_family == AF_INET) {
            return NetworkAddress::Type::IPV4;
        }
        else if (_impl->_address.ss_family == AF_INET6) {
            return NetworkAddress::Type::IPV6;
        }
        else {
            return NetworkAddress::Type::Unknown;
        }
    }
    std::string NetworkAddress::get_addr() const {
        if (_impl->_address.ss_family == AF_INET) {
            std::array<char, 16> res;
            inet_ntop(AF_INET, &((sockaddr_in*)(&_impl->_address))->sin_addr, res.data(), res.size());
            std::string res_str(res.data());
            return res_str;
        }
        if (_impl->_address.ss_family == AF_INET6) {
            std::array<char, 46> res;
            inet_ntop(AF_INET6, &((sockaddr_in6*)(&_impl->_address))->sin6_addr, res.data(), res.size());
            std::string res_str(res.data());
            return res_str;
        }
        return std::string();
    }
    uint16_t NetworkAddress::get_port() const {
        if (_impl->_address.ss_family == AF_INET) {
            return ntohs(((sockaddr_in*)(&_impl->_address))->sin_port);
        }
        if (_impl->_address.ss_family == AF_INET6) {
            return ntohs(((sockaddr_in6*)(&_impl->_address))->sin6_port);
        }
        return 0;
    }
    std::string NetworkAddress::get_full_addr() const {
        if (get_type() == NetworkAddress::Type::IPV6) {
            //IPV6 format [address]:port
            return "[" + get_addr() + "]:" + std::to_string(get_port());
        }
        else {
            //IPV4 format address:port
            return get_addr() + ":" + std::to_string(get_port());
        }
    }
    NetworkAddressOSData* NetworkAddress::get_impl() const {
        return _impl;
    }

    bool NetworkAddress::operator==(const NetworkAddress& rhs) const {
        if (_impl->_address_length != rhs._impl->_address_length) return false;
        return memcmp(&_impl->_address, &rhs._impl->_address, _impl->_address_length) == 0;
    }
    bool NetworkAddress::operator!=(const NetworkAddress& rhs) const {
        return !(this->operator==(rhs));
    }

    NetworkAddress::~NetworkAddress() {
        delete _impl;
        _impl = nullptr;
    }

    std::ostream& operator<<(std::ostream& os, const NetworkAddress& addr)
    {
        os << addr.get_full_addr();
        return os;
    }

    //SocketOSData

    SocketOSData::SocketOSData(SOCKET socket) : _socket(socket) {

    }
    SocketOSData::SocketOSData(SocketOSData&& other) noexcept {
        _socket = other._socket;
        other._socket = INVALID_SOCKET;
    }
    SocketOSData& SocketOSData::operator=(SocketOSData&& other) noexcept {
        close();
        _socket = other._socket;
        other._socket = INVALID_SOCKET;
        return *this;
    }
    inline void SocketOSData::set(SOCKET socket) {
        close();
        _socket = socket;
    }

    inline SOCKET SocketOSData::get() {
        return _socket;
    }

    inline bool SocketOSData::is_valid() const {
        return _socket != INVALID_SOCKET;
    }
    inline bool SocketOSData::is_invalid() const {
        return _socket == INVALID_SOCKET;
    }

    void SocketOSData::set_events(HANDLE& hwnd) {
        assert(hwnd != INVALID_HANDLE_VALUE);
        int res = WSAEventSelect(
            _socket,
            hwnd,
            FD_READ | FD_ACCEPT | FD_CONNECT | FD_CLOSE);
        assert(res == 0);
    }

    inline void SocketOSData::close() {
        if (is_valid()) {
            //std::cout << "CLOSE SOCKET" << std::endl;
            closesocket(_socket);
            _socket = INVALID_SOCKET;
        }
    }
    SocketOSData::~SocketOSData() {
        close();
    }

    //SocketBase

    SocketBase::SocketBase() : _socket(new SocketOSData()) {

    }
    SocketBase::SocketBase(SocketBase&& other) noexcept : _socket(other._socket) {
        other._socket = new SocketOSData();

    }
    SocketBase& SocketBase::operator=(SocketBase&& other) noexcept {
        delete _socket;
        _socket = other._socket;
        other._socket = new SocketOSData();
        return *this;
    }


    bool SocketBase::is_valid() {
        return _socket->is_valid();
    }
    bool SocketBase::is_invalid() {
        return _socket->is_invalid();
    }

    inline void SocketBase::close() {
        return _socket->close();
    }

    SocketBase::~SocketBase() {
        delete _socket;
        _socket = nullptr;
    }

    //SocketEventHandle

    //TCPS

    TCPS::TCPS() : SocketEventHandle(Type::TCPS) {

    }

    ErrorCode TCPS::bind(const std::string& name, uint16_t port) {
        int iResult;

        // Protocol
        struct addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        // Resolve the server address and port
        std::string port_s = std::to_string(port);
        addrinfo* result = nullptr;

        iResult = ::getaddrinfo((name.size() == 0) ? nullptr : name.c_str(), port_s.c_str(), &hints, &result);
        if (iResult != 0) {
            return ErrorCode::NETWORK_ERROR_RESOLVE_HOSTNAME;
        }

        // Create the server listen socket
        _socket->set(::socket(result->ai_family, result->ai_socktype, result->ai_protocol));
        if (is_invalid()) {
            freeaddrinfo(result);
            return ErrorCode::NETWORK_ERROR_CREATE_SOCKET;
        }

        // Bind the listen socket
        iResult = ::bind(_socket->get(), result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            freeaddrinfo(result);
            _socket->close();
            return ErrorCode::NETWORK_ERROR_SERVER_BIND;
        }

        freeaddrinfo(result);

        // Set socket to listening mode
        iResult = ::listen(_socket->get(), SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            _socket->close();
            return ErrorCode::NETWORK_ERROR_SERVER_LISTEN;
        }

        return ErrorCode::OK;
    }

    void TCPS::start() {
        _base->start_socket(shared_from_this());
    }
    bool TCPS::close() {
        return _base->close_socket(shared_from_this());
    }

    TCPC::TCPC() : SocketEventHandle(Type::TCPC) {

    }
    TCPC::TCPC(SocketOSData* socket, NetworkAddress&& remote_address) : SocketEventHandle(Type::TCPC, socket) {
        _remote_address = std::move(remote_address);
    }

    ErrorCode TCPC::resolve(const std::string& name, uint16_t port) {
        /*NetworkAddress local_addr;

        _socket.set(::socket(AF_INET6, SOCK_STREAM, 0));
        if (_socket.is_invalid()) {
            NetworkError(NetworkErrorCodeCreateClientSocket, WSAGetLastError());
            return;
        }

        int ipv6only = 0;
        int iResult = setsockopt(_socket.get(), IPPROTO_IPV6,
            IPV6_V6ONLY, (char*)&ipv6only, sizeof(ipv6only));
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeCreateClientSocket, WSAGetLastError());
            _socket.close();
            return;
        }

        std::string port_s = std::to_string(port);
        BOOL bSuccess = WSAConnectByName(
            _socket.get(),
            name.c_str(),
            port_s.c_str(),
            local_addr.sizew(),
            (SOCKADDR*)local_addr.getw(),
            _remote_addr.sizew(),
            (SOCKADDR*)_remote_addr.getw(),
            NULL,
            NULL);
        if (!bSuccess) {
            NetworkError(NetworkErrorCodeConnect, WSAGetLastError());
            _socket.close();
            return;
        }

        iResult = setsockopt(_socket.get(), SOL_SOCKET,
            SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        if (iResult == SOCKET_ERROR) {
            wprintf(L"setsockopt for SO_UPDATE_CONNECT_CONTEXT failed with error: %d\n",
                WSAGetLastError());
            _socket.close();
            return;
        }*/


        int iResult;

        // Protocol
        struct addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        // Resolve the server address and port
        std::string port_s = std::to_string(port);
        addrinfo* result = nullptr;
        iResult = ::getaddrinfo(name.c_str(), port_s.c_str(), &hints, &result);
        if (iResult != 0) {
            return ErrorCode::NETWORK_ERROR_RESOLVE_HOSTNAME;
        }

        _addrs = (AddrInfoOSData*)result;
        _addr_current = _addrs;
        /*
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        // Create socket
        _socket.set(::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol));
        if (_socket.invalid()) {
            NetworkError(NetworkErrorCodeCreateClientSocket, WSAGetLastError());
            return;
        }

        // Connect to server.
        iResult = ::connect(_socket.get(), ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            _socket.close();
            continue;
        }
        break;
    }
    */

    //No address found
        if (_addrs == nullptr) return ErrorCode::NETWORK_ERROR_RESOLVE_HOSTNAME;

        return ErrorCode::OK;
    }

    void TCPC::send(Utils::ConstBlobView& data) {
        ::send(_socket->get(), (const char*)data.getr(), data.size(), 0);
    }

    void TCPC::start() {
        _base->start_socket(shared_from_this());
    }
    bool TCPC::close() {
        return _base->close_socket(shared_from_this());
    }

    ErrorCode TCPC::next_connect_attempt(EventHandleOSData* event_binder_wrapper) {
        if (_addr_current == nullptr) {
            if (_addrs != nullptr) {
                freeaddrinfo(_addrs);
                _addrs = nullptr;
            }
            return ErrorCode::NETWORK_ERROR_CONNECT;
        }

        HANDLE event_binder = event_binder_wrapper->hwnd;

        if (_socket->is_invalid()) {
            _socket->set(::socket(
                _addr_current->ai_family,
                _addr_current->ai_socktype,
                _addr_current->ai_protocol));
            if (_socket->is_invalid()) {
                _addr_current = nullptr;
                if (_addrs != nullptr) {
                    freeaddrinfo(_addrs);
                    _addrs = nullptr;
                }
                return ErrorCode::NETWORK_ERROR_CREATE_SOCKET;
            }
            _socket->set_events(event_binder);
        }

        // Connect to server.
        int iResult = WSAConnect(
            _socket->get(),
            _addr_current->ai_addr,
            (int)_addr_current->ai_addrlen,
            NULL, NULL, NULL, NULL);
        //Store connected address
        *_remote_address._impl->sizew() = (int)_addr_current->ai_addrlen;
        memcpy(
            _remote_address._impl->get_dataw(),
            _addr_current->ai_addr,
            (int)_addr_current->ai_addrlen);

        if(iResult == 0) {
            //Success, wipe addrinfo
            _addr_current = nullptr;
            if (_addrs != nullptr) {
                freeaddrinfo(_addrs);
                _addrs = nullptr;
            }
            return ErrorCode::OK;
        }
        else {
            int wsa_error = WSAGetLastError();
            //Advance to next
            _addr_current = (AddrInfoOSData*)_addr_current->ai_next;
            if (_addr_current == nullptr) {
                if (_addrs != nullptr) {
                    freeaddrinfo(_addrs);
                    _addrs = nullptr;
                }
            }
            if (wsa_error == WSAEWOULDBLOCK) {
                return ErrorCode::NETWORK_WARN_CONNECTING;
            }
            else {
                _addr_current = nullptr;
                if (_addrs != nullptr) {
                    freeaddrinfo(_addrs);
                    _addrs = nullptr;
                }
                _socket->close();
                return ErrorCode::NETWORK_ERROR_CONNECT;
            }
        }
    }

    TCPC::~TCPC() {
        if (_addrs != nullptr) {
            freeaddrinfo(_addrs);
            _addrs = nullptr;
        }
    }

    UDP::UDP() : SocketEventHandle(Type::UDP) {

    }

    ErrorCode UDP::bind(NetworkAddress&& local) {
        int iResult;

        _local_address = std::move(local);

        // Create the UDP socket
        _socket->set(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        if (_socket->is_invalid()) {
            return ErrorCode::NETWORK_ERROR_CREATE_SOCKET;
        }

        // Receive on its own outbound address
        iResult = ::bind(_socket->get(), (const sockaddr*)_local_address.get_impl()->get_data(), _local_address.get_impl()->size());
        if (iResult == SOCKET_ERROR) {
            _socket->close();
            return ErrorCode::NETWORK_ERROR_SERVER_BIND;
        }
        return ErrorCode::OK;
    }

    void UDP::send(Utils::ConstBlobView& data) {
        ::sendto(_socket->get(), (const char*)data.getr(), data.size(), 0, (sockaddr*)_remote_address.get_impl()->get_data(), _remote_address.get_impl()->size());
    }

    void UDP::start() {
        _base->start_socket(shared_from_this());
    }
    bool UDP::close() {
        return _base->close_socket(shared_from_this());
    }

    //SocketEventEmitterImpl

    void __stdcall SocketEventEmitterProviderImpl::apc_fun(ULONG_PTR data) {
        (void)data;
    }

    void SocketEventEmitterProviderImpl::bind() {
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
    void SocketEventEmitterProviderImpl::wait(Time::time_delta_type_us delay) {
        std::lock_guard _lg(_sockets_lock);

        int64_t delay_ms = (delay + 999) / 1000;
        DWORD timeout = ((delay_ms < std::numeric_limits<DWORD>::max()) ? (DWORD)delay_ms : INFINITE);
        if (delay < 0) timeout = INFINITE;

        DWORD ncount = (DWORD)_socket_events.size();

        assert(_socket_events.size() == _socket_objects.size());

        //std::cout << "Wait start on " << ncount << " sockets" << std::endl;

        if (ncount == 0) {
            SleepEx(timeout, TRUE);
        }
        else {
            DWORD res = WSAWaitForMultipleEvents(
                ncount,
                _socket_events.data(),
                FALSE, timeout, TRUE);

            if (res == WSA_WAIT_FAILED) {
                //std::cout << "WSA Wait failed " << WSAGetLastError() << std::endl;
            }
            else if (res == WAIT_IO_COMPLETION) {
                //std::cout << "Wait abort" << std::endl;
                //APC triggered
            }
            else if (res == WAIT_TIMEOUT) {
                //std::cout << "Wait timeout" << std::endl;
                //Timeout triggered
            }
            else if (WAIT_OBJECT_0 <= res && res < (WAIT_OBJECT_0 + ncount)) {
                int index = res - WAIT_OBJECT_0;
                //std::cout << "Wait result " << index << std::endl;

                std::shared_ptr<SocketEventHandle> socket_hwnd = _socket_objects[index];
                SOCKET socket = socket_hwnd->_socket->get();

                WSANETWORKEVENTS set_events;
                //TODO: Handle errors
                int res2 = WSAEnumNetworkEvents(
                    socket,
                    _socket_events[index],
                    &set_events);
                (void)res2;

                /*bool bResult = WSAResetEvent(_socket_events[index]);
                if (bResult == FALSE) {
                    std::cout << "Event reset error" << std::endl;
                }*/

                if (set_events.lNetworkEvents & FD_CONNECT) {
                    int error = set_events.iErrorCode[FD_CONNECT_BIT];
                    if (socket_hwnd->_type == SocketEventHandle::Type::TCPC) {
                        if (error == 0) {
                            std::cout << "Connect success" << std::endl;
                            _socket_objects[index]->_callback_connect();
                        }
                        else {
                            std::cout << "Connect error " << error << std::endl;
                            std::cout << "Next attempt..." << std::endl;
                            EventHandleOSData hwnd_wrapper;
                            hwnd_wrapper.hwnd = _socket_events[index];
                            ErrorCode code = socket_hwnd->next_connect_attempt(&hwnd_wrapper);
                            if (ErrorCodes::is_error(code)) {
                                socket_hwnd->_callback_error(code);
                                _closed_socket_objects.push_back(socket_hwnd);
                            }
                        }
                    }
                    else {
                        assert(false);
                    }
                }

                if (set_events.lNetworkEvents & FD_READ) {
                    //std::cout << "Readable" << std::endl;
                    if (socket_hwnd->_type == SocketEventHandle::Type::TCPC) {
                        Utils::Blob data = Utils::Blob::factory_empty(socket_hwnd->_recvbuf_len);
                        WSABUF buffer;
                        buffer.buf = (CHAR*)data.getw();
                        buffer.len = data.size();
                        DWORD received;
                        DWORD flags = 0;
                        int res3 = WSARecv(
                            socket,
                            &buffer,
                            1,
                            &received,
                            &flags,
                            NULL,
                            NULL);
                        //TODO
                        (void)res3;

                        //Closed
                        if (received == 0) {
                            std::lock_guard _lg2(_system_lock);
                            _closed_socket_objects.push_back(socket_hwnd);
                        }
                        else {
                            data.resize(received);
                            _socket_objects[index]->_callback_packet(data);
                        }
                    }
                    else if (socket_hwnd->_type == SocketEventHandle::Type::UDP) {
                        Utils::Blob data = Utils::Blob::factory_empty(socket_hwnd->_recvbuf_len);
                        NetworkAddress source;

                        WSABUF buffer;
                        buffer.buf = (CHAR*)data.getw();
                        buffer.len = data.size();
                        DWORD received;
                        DWORD flags = 0;
                        int res3 = WSARecvFrom(
                            socket,
                            &buffer,
                            1,
                            &received,
                            &flags,
                            (sockaddr*)source.get_impl()->get_dataw(),
                            source.get_impl()->sizew(),
                            NULL,
                            NULL
                        );

                        if (res3 == 0) {
                            //Closed
                            if (received == 0) {
                                std::lock_guard _lg2(_system_lock);
                                _closed_socket_objects.push_back(socket_hwnd);
                            }
                            else {
                                data.resize(received);
                                if (source == socket_hwnd->_remote_address) {
                                    socket_hwnd->_callback_packet(data);
                                }
                                socket_hwnd->_callback_unfiltered_packet(data, source);
                            }
                        }
                        else {
                            int wsa_error = WSAGetLastError();
                            if (wsa_error != WSAECONNRESET) {
                                std::cout << wsa_error << std::endl;
                                assert(false);
                            }
                        }
                    }
                    else {
                        assert(false);
                    }
                }

                if (set_events.lNetworkEvents & FD_ACCEPT) {
                    if (socket_hwnd->_type == SocketEventHandle::Type::TCPS) {
                        std::cout << "Accept" << std::endl;
                        NetworkAddress remote_address;

                        // Accept a client socket
                        int len = remote_address.get_impl()->size();
                        SOCKET client = WSAAccept(socket, (sockaddr*)remote_address.get_impl()->get_dataw(), &len, NULL, NULL);
                        *remote_address.get_impl()->sizew() = len;

                        if (client == INVALID_SOCKET) {
                            int error = WSAGetLastError();
                            if (error != WSAEWOULDBLOCK && error != WSAECONNRESET) {
                                close_socket(socket_hwnd);
                            }
                        }
                        else {
                            SocketOSData* accepted = new SocketOSData(client);
                            TCPCHandleU new_client(new TCPC(accepted, std::move(remote_address)));

                            _socket_objects[index]->_callback_accept(std::move(new_client));
                        }
                    }
                    else {
                        assert(false);
                    }
                }

                if (set_events.lNetworkEvents & FD_CLOSE) {
                    std::cout << "Close" << std::endl;
                    std::lock_guard _lg2(_system_lock);
                    _closed_socket_objects.push_back(socket_hwnd);
                }
            }
        }

        //Execute updates
        {
            std::unique_lock _lg2(_system_lock);

            while (_closed_socket_objects.size() || _added_socket_objects.size()) {
                while (_closed_socket_objects.size()) {
                    std::shared_ptr<SocketEventHandle> close_socket = _closed_socket_objects.front();
                    _closed_socket_objects.pop_front();

                    _lg2.unlock();

                    for (int i = 0; i < _socket_objects.size(); i++) {
                        if (_socket_objects[i] == close_socket) {
                            //Close socket and event
                            std::cout << "Closing socket to " << _socket_objects[i]->get_remote() << std::endl;
                            _socket_objects[i]->_socket->close();
                            WSACloseEvent(_socket_events[i]);
                            _socket_objects[i]->_callback_close();

                            //Move to front
                            _socket_objects[i] = _socket_objects[_socket_objects.size() - 1];
                            _socket_objects.resize(_socket_objects.size() - 1);
                            _socket_events[i] = _socket_events[_socket_events.size() - 1];
                            _socket_events.resize(_socket_events.size() - 1);

                            assert(_socket_events.size() == _socket_objects.size());
                            break;
                        }
                    }

                    _lg2.lock();
                }

                while (_added_socket_objects.size()) {
                    std::shared_ptr<SocketEventHandle> add_socket = _added_socket_objects.front();
                    _added_socket_objects.pop_front();

                    _lg2.unlock();

                    //Create events objects
                    HANDLE new_event = WSACreateEvent();
                    assert(new_event != WSA_INVALID_EVENT);

                    //std::cout << "Added socket" << std::endl;

                    if (add_socket->_type == SocketEventHandle::Type::TCPC && add_socket->is_invalid()) {
                        EventHandleOSData hwnd_wrapper;
                        hwnd_wrapper.hwnd = new_event;
                        ErrorCode code = add_socket->next_connect_attempt(&hwnd_wrapper);
                        if (ErrorCodes::is_error(code)) {
                            add_socket->_callback_error(code);
                            _closed_socket_objects.push_back(add_socket);
                            _lg2.lock();
                            continue;
                        }
                    }
                    else {
                        //Bind to socket
                        add_socket->_socket->set_events(new_event);
                    }

                    _socket_events.push_back(new_event);
                    _socket_objects.push_back(add_socket);
                    assert(_socket_events.size() == _socket_objects.size());

                    add_socket->_callback_start();

                    _lg2.lock();
                }
            }
        }
    }

    void SocketEventEmitterProviderImpl::run_now(Utils::Callback<>::raw_type fn) {
        std::lock_guard _lg(_system_lock);
        _tasks.emplace_back(fn);
        assert(_this_thread != INVALID_HANDLE_VALUE);
        QueueUserAPC(SocketEventEmitterProviderImpl::apc_fun, _this_thread, 0);
    }
    void SocketEventEmitterProviderImpl::interrupt() {
        std::lock_guard _lg(_system_lock);
        assert(_this_thread != INVALID_HANDLE_VALUE);
        QueueUserAPC(SocketEventEmitterProviderImpl::apc_fun, _this_thread, 0);
    }

    void SocketEventEmitterProviderImpl::start_socket(std::shared_ptr<SocketEventHandle> socket) {
        std::lock_guard _lg(_system_lock);
        _added_socket_objects.push_back(socket);
        assert(_this_thread != INVALID_HANDLE_VALUE);
        QueueUserAPC(SocketEventEmitterProviderImpl::apc_fun, _this_thread, 0);
    }
    void SocketEventEmitterProviderImpl::close_socket(std::shared_ptr<SocketEventHandle> socket) {
        std::lock_guard _lg(_system_lock);
        _closed_socket_objects.push_back(socket);
        assert(_this_thread != INVALID_HANDLE_VALUE);
        QueueUserAPC(SocketEventEmitterProviderImpl::apc_fun, _this_thread, 0);
    }
    bool SocketEventEmitterProviderImpl::extract_socket(std::shared_ptr<SocketEventHandle> socket) {
        std::lock_guard _lg(_sockets_lock);

        bool success = false;

        for (int i = 0; i < _socket_objects.size(); i++) {
            if (_socket_objects[i] == socket) {
                //Close socket and event
                _socket_objects[i]->close();
                WSACloseEvent(_socket_events[i]);
                _socket_objects[i]->_callback_close();

                //Move to front
                _socket_objects[i] = _socket_objects[_socket_objects.size() - 1];
                _socket_objects.resize(_socket_objects.size() - 1);
                _socket_events[i] = _socket_events[_socket_events.size() - 1];
                _socket_events.resize(_socket_events.size() - 1);

                assert(_socket_events.size() == _socket_objects.size());
                //Should only delete once
                assert(success == false);
                success = true;
            }
        }

        return success;
    }

    //SocketEventEmitterProvider

    const int SocketEventEmitterProvider::MAX_SOCKETS = MAXIMUM_WAIT_OBJECTS;

    SocketEventEmitterProvider::SocketEventEmitterProvider() : _impl(new SocketEventEmitterProviderImpl()) {

    }

    SocketEventEmitterProvider::~SocketEventEmitterProvider() {
        delete _impl;
        _impl = nullptr;
    }

    void SocketEventEmitterProvider::bind() {
        _impl->bind();
    }
    void SocketEventEmitterProvider::wait(Time::time_delta_type_us delay) {
        _impl->wait(delay);
    }

    void SocketEventEmitterProvider::run_now(Utils::Callback<>::raw_type fn) {
        _impl->run_now(fn);
    }
    void SocketEventEmitterProvider::interrupt() {
        _impl->interrupt();
    }

    void SocketEventEmitterProvider::associate_socket(TCPSHandleU&& hwnd) {
        std::lock_guard _lg(_sockets_lock);

        TCPSHandleS hwnds = hwnd;
        hwnds->_self = _sockets_tcps.emplace(_sockets_tcps.end(), std::move(hwnd));
        hwnds->_base = shared_from_this();
    }
    void SocketEventEmitterProvider::associate_socket(TCPCHandleU&& hwnd) {
        std::lock_guard _lg(_sockets_lock);

        TCPCHandleS hwnds = hwnd;
        hwnds->_self = _sockets_tcpc.emplace(_sockets_tcpc.end(), std::move(hwnd));
        hwnds->_base = shared_from_this();
    }
    void SocketEventEmitterProvider::associate_socket(UDPHandleU&& hwnd) {
        std::lock_guard _lg(_sockets_lock);

        UDPHandleS hwnds = hwnd;
        hwnds->_self = _sockets_udp.emplace(_sockets_udp.end(), std::move(hwnd));
        hwnds->_base = shared_from_this();
    }

    bool SocketEventEmitterProvider::start_socket(std::shared_ptr<SocketEventHandle> hwnd) {
        std::lock_guard _lg(_sockets_lock);

        _impl->start_socket(hwnd);

        //Todo: make sure socket is assocaited with this
        return true;
    }

    bool SocketEventEmitterProvider::close_socket(TCPSHandleS hwnd) {
        std::lock_guard _lg(_sockets_lock);

        if (hwnd->_self == _sockets_tcps.end()) return false;
        _sockets_tcps.erase(hwnd->_self);
        hwnd->_self = _sockets_tcps.end();
        _impl->close_socket(hwnd);
        return true;

    }
    bool SocketEventEmitterProvider::close_socket(TCPCHandleS hwnd) {
        std::lock_guard _lg(_sockets_lock);

        if (hwnd->_self == _sockets_tcpc.end()) return false;
        _sockets_tcpc.erase(hwnd->_self);
        hwnd->_self = _sockets_tcpc.end();
        _impl->close_socket(hwnd);
        return true;
    }
    bool SocketEventEmitterProvider::close_socket(UDPHandleS hwnd) {
        std::lock_guard _lg(_sockets_lock);

        if (hwnd->_self == _sockets_udp.end()) return false;
        _sockets_udp.erase(hwnd->_self);
        hwnd->_self = _sockets_udp.end();
        _impl->close_socket(hwnd);
        return true;
    }

    TCPSHandleU SocketEventEmitterProvider::extract_socket(TCPSHandleS hwnd) {
        std::lock_guard _lg(_sockets_lock);

        if (hwnd->_self == _sockets_tcps.end()) return TCPSHandleU();
        TCPSHandleU socket = std::move(*hwnd->_self);
        _sockets_tcps.erase(hwnd->_self);
        hwnd->_self = _sockets_tcps.end();
        return socket;
    }
    TCPCHandleU SocketEventEmitterProvider::extract_socket(TCPCHandleS hwnd) {
        std::lock_guard _lg(_sockets_lock);

        if (hwnd->_self == _sockets_tcpc.end()) return TCPCHandleU();
        TCPCHandleU socket = std::move(*hwnd->_self);
        _sockets_tcpc.erase(hwnd->_self);
        hwnd->_self = _sockets_tcpc.end();
        return socket;
    }
    UDPHandleU SocketEventEmitterProvider::extract_socket(UDPHandleS hwnd) {
        std::lock_guard _lg(_sockets_lock);

        if (hwnd->_self == _sockets_udp.end()) return UDPHandleU();
        UDPHandleU socket = std::move(*hwnd->_self);
        _sockets_udp.erase(hwnd->_self);
        hwnd->_self = _sockets_udp.end();
        return socket;
    }

    size_t SocketEventEmitterProvider::count() {
        std::lock_guard _lg(_sockets_lock);
        return _sockets_tcps.size() + _sockets_tcpc.size() + _sockets_udp.size();
    }
}

#endif