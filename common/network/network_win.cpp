#ifdef WIN32

#include "network_win.h"

namespace NATBuster::Common::Network {
    namespace WSA {
        //Class to hold the WSA instance
        class WSAContainer;
        class WSAWrapper : Utils::NonCopyable {
            WSADATA _wsa_data;

            WSAWrapper(uint8_t major = 2, uint8_t minor = 2) {
                //Create an instance of winsock
                int iResult = WSAStartup(MAKEWORD(major, minor), &_wsa_data);
                if (iResult != 0) {
                    NetworkError(NetworkErrorCodeInitalize, iResult);
                    return;
                }
            }

            ~WSAWrapper() {
                WSACleanup();
            }

            friend class WSAContainer;

        public:
            WSADATA get() const {
                return _wsa_data;
            }
        };

        class WSAContainer {
        public:
            static const WSAWrapper wsa;
        };

        const WSAWrapper WSAContainer::wsa(2, 2);
    }

    //Log errors
    void NetworkError(NetworkErrorCodes internal_id, int os_id) {
        std::cerr << "Error code: " << internal_id << ", " << os_id << std::endl;
    }

    NetworkAddress::NetworkAddress() {
        ZeroMemory(&_address, sizeof(sockaddr_in));
    }

    NetworkAddress::NetworkAddress(std::string name, uint16_t port) {
        ZeroMemory(&_address, sizeof(sockaddr_in));

        if (name.length()) {
            //Resolve host name to IP
            hostent* server_ip = gethostbyname(name.c_str());

            if (server_ip == NULL) {
                NetworkError(NetworkErrorCodeResolveAddress, WSAGetLastError());
                return;
            }

            if ((server_ip->h_addrtype != AF_INET) || (server_ip->h_addr_list[0] == 0)) {
                NetworkError(NetworkErrorCodeResolveAddress, WSAGetLastError());
                return;
            }

            _address.sin_addr.s_addr = *(u_long*)server_ip->h_addr_list[0];
        }
        else {
            //Any address
            _address.sin_addr.s_addr = INADDR_ANY;
        }

        //Address
        _address.sin_family = AF_INET;
        _address.sin_port = htons(port);
    }

    template <typename MY_HWND>
    bool SocketBase<MY_HWND>::check(Timeout timeout) {
        FD_SET collection;

        FD_ZERO(&collection);

        if (_socket.valid()) {
            FD_SET(sw.get(), &collection);
        }

        int count = select(0, &collection, NULL, NULL, to.to_native());

        if (count == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeSelectRead, WSAGetLastError());
            return false;
        }

        if (count == 0) {
            return false;
        }

        if (sw.valid()) {
            if (FD_ISSET(socket->_socket.get(), &collection)) {
                return true;
            }
        }

        return false;
    }

    template <typename MY_HWND>
    MY_HWND SocketBase<MY_HWND>::find(const std::list<MY_HWND>& sockets, Timeout timeout) {
        FD_SET collection;

        FD_ZERO(&collection);

        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_socket;

            if (sw.valid()) {
                FD_SET(sw.get(), &collection);
            }
        }

        timeval to = timeout.to_native();

        int count = select(0, &collection, NULL, NULL, (timeout.infinite()) ? NULL : (&to));

        if (count == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeSelectRead, WSAGetLastError());
            return TCPSHandle();
        }

        if (count == 0) {
            return TCPSHandle();
        }

        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_socket;

            if (sw.valid()) {
                if (FD_ISSET(socket->_socket.get(), &collection)) {
                    return socket;
                }
            }
        }
        return TCPSHandle();
    }


    //
    // TCP Server OS implementation
    // 

    TCPS::TCPS(std::string name, uint16_t port) {
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
        addrinfo* result = NULL;
        iResult = ::getaddrinfo(NULL, port_s.c_str(), &hints, &result);
        if (iResult != 0) {
            NetworkError(NetworkErrorCodeResolveAddress, iResult);
            return;
        }

        // Create the server listen socket
        _socket.set(::socket(result->ai_family, result->ai_socktype, result->ai_protocol));
        if (_socket.invalid()) {
            NetworkError(NetworkErrorCodeCreateListenSocket, WSAGetLastError());
            freeaddrinfo(result);
            return;
        }

        // Bind the listen socket
        iResult = ::bind(_socket.get(), result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeBindListenSocket, WSAGetLastError());
            freeaddrinfo(result);
            _socket.close();
            return;
        }

        freeaddrinfo(result);

        // Set socket to listening mode
        iResult = ::listen(_socket.get(), SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeListen, WSAGetLastError());
            _socket.close();
            return;
        }
    }

    TCPCHandle TCPS::accept() {
        int iResult;
        SOCKET clientSocket;

        // Accept a client socket
        clientSocket = ::accept(_socket.get(), NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            int error = WSAGetLastError();
            NetworkError(NetworkErrorCodeAccept, error);
            //Connection closed by remote before it could be accepted: Not a fatal error
            if (error != WSAECONNRESET) {
                _socket.close();
            }

            return std::shared_ptr<TCPC>();
        }

        return std::make_shared<TCPC>(clientSocket);
    }

    TCPS::~TCPS() {
        _socket.close();
    }

    //
    // TCP Client OS implementation
    //

    TCPC::TCPC(std::string name, uint16_t port) {
        int iResult;

        // Protocol
        struct addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        // Resolve the server address and port
        std::string port_s = std::to_string(port);
        addrinfo* result = NULL;
        iResult = ::getaddrinfo(name.c_str(), port_s.c_str(), &hints, &result);
        if (iResult != 0) {
            NetworkError(NetworkErrorCodeResolveAddress, iResult);
            return;
        }

        // Find potential addresses
        for (addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
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

        freeaddrinfo(result);

        if (_socket.invalid()) {
            NetworkError(NetworkErrorCodeConnect, 0);
            return;
        }
    }

    TCPC::TCPC(SOCKET client) : SocketBase<TCPCHandle>(client) {

    }

    bool TCPC::send(Packet data) {
        if (_socket.invalid()) return false;

        int progress = 0;

        while (progress < data.size()) {
            //Send bytes till all sent
            int iSendResult = ::send(_socket.get(), (char*)(data.get() + progress), data.size() - progress, 0);

            //Stop if error
            if (iSendResult == SOCKET_ERROR) {
                NetworkError(NetworkErrorCodeSendData, WSAGetLastError());
                _socket.close();
                return false;
            }

            //Stop if no progress was made
            if (iSendResult == 0) {
                _socket.close();
                return false;
            }

            progress += iSendResult;
        }
        return true;
    }

    Packet TCPC::read(uint32_t min_len, uint32_t max_len) {
        uint8_t* res = new uint8_t(max_len);

        int progress = 0;

        //Even if min 0 requested, still do one round
        do {
            int recv_len = recv(_socket.get(), (char*)res, max_len, 0);

            //Error
            if (recv_len == SOCKET_ERROR) {
                NetworkError(NetworkErrorCodeReciveData, WSAGetLastError());
                delete[] res;
                return Packet();
            }

            //Connection gracefully closed
            if (recv_len == 0) {
                delete[] res;
                return Packet();
            }

            progress += recv_len;
        } while (progress < min_len);

        return Packet::consume_from(progress, res);
    }

    //
    // UDP
    //

    UDP::UDP(std::string name, uint16_t remote_port, uint16_t local_port) : UDP(NetworkAddress(name, remote_port), NetworkAddress("", local_port)) {

    }

    UDP::UDP(NetworkAddress remote_address, NetworkAddress local_address) : _remote_address(remote_address), _local_address(local_address) {
        int iResult;

        // Create the UDP socket
        _socket.set(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        if (_socket.invalid()) {
            NetworkError(NetworkErrorCodeCreateClientSocket, WSAGetLastError());
            return;
        }

        // Receive on its own outbound address
        iResult = ::bind(_socket.get(), _local_address.get(), _local_address.size());
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeBindListenSocket, WSAGetLastError());
            _socket.close();
            return;
        }
    }

    bool UDP::send(Packet data) {
        // Send a datagram to the receiver
        int iResult = ::sendto(_socket.get(),
            (char*)data.get(), data.size(), 0, _remote_address.get(), _remote_address.size());
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeSendData, WSAGetLastError());
            _socket.close();
            return false;
        }
        return true;
    }

    Packet UDP::readFilter(uint32_t max_len) {
        NetworkAddress src;
        Packet res = read(max_len, src);

        if (src == _remote_address) {
            return res;
        }
        return Packet();
    }

    Packet UDP::read(uint32_t max_len, NetworkAddress& address) {
        uint8_t* recv_buf = new uint8_t(max_len);

        int size = address.size();
        int iResult = recvfrom(_socket.get(), (char*)recv_buf, max_len, 0, address.get(), &size);
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeReciveData, WSAGetLastError());
            _socket.close();
        }

        // make the buffer zero terminated
        return Packet::consume_from(iResult, recv_buf);
    }

    UDP::~UDP() {
        _socket.close();
    }

}

#endif