#ifdef WIN32

#include "network_win.h"

namespace NATBuster::Common::Network {
    //Log errors
    void NetworkError(NetworkErrorCodes internal_id, int os_id) {

        wchar_t* s = NULL;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, os_id,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&s, 0, NULL);

        std::cerr << "Error code: " << internal_id << ", " << os_id << ":";
        std::wcerr << s;
        std::cerr << std::endl;
        
        LocalFree(s);

    }

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
                std::cout << "WSA CLEAN" << std::endl;
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
    NetworkAddress::NetworkAddress() {
        ZeroMemory(&_address, sizeof(sockaddr_in));
    }

    NetworkAddress::NetworkAddress(const std::string& name, uint16_t port) {
        ZeroMemory(&_address, sizeof(SOCKADDR_STORAGE));

        if (name.length()) {
            //Resolve host name to IP
            hostent* server_ip = gethostbyname(name.c_str());

            if (server_ip == nullptr) {
                NetworkError(NetworkErrorCodeResolveAddress, WSAGetLastError());
                return;
            }

            if ((server_ip->h_addrtype != AF_INET) || (server_ip->h_addr_list[0] == 0)) {
                NetworkError(NetworkErrorCodeResolveAddress, WSAGetLastError());
                return;
            }

            ((sockaddr_in*)&_address)->sin_addr.s_addr = *(u_long*)server_ip->h_addr_list[0];
        }
        else {
            //Any address
            ((sockaddr_in*)&_address)->sin_addr.s_addr = INADDR_ANY;
        }

        //Address
        _address.ss_family = AF_INET;
        ((sockaddr_in*)&_address)->sin_family = AF_INET;
        ((sockaddr_in*)&_address)->sin_port = htons(port);
    }

    //
    // TCP Server OS implementation
    // 

    TCPS::TCPS(const std::string& name, uint16_t port) {
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
        iResult = ::getaddrinfo(nullptr, port_s.c_str(), &hints, &result);
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
        NetworkAddress remote_address;
        SOCKET clientSocket;

        // Accept a client socket
        int len = *remote_address.sizew();
        clientSocket = ::accept(_socket.get(), (sockaddr*)remote_address.getw(), &len);
        *remote_address.sizew() = len;
        if (clientSocket == INVALID_SOCKET) {
            int error = WSAGetLastError();
            NetworkError(NetworkErrorCodeAccept, error);
            //Connection closed by remote before it could be accepted: Not a fatal error
            if (error != WSAECONNRESET) {
                _socket.close();
            }

            return std::shared_ptr<TCPC>();
        }

        TCPC* newSocket = new TCPC(clientSocket, remote_address);

        return TCPCHandle(newSocket);
    }

    TCPS::~TCPS() {
        _socket.close();
    }

    //
    // TCP Client OS implementation
    //

    TCPC::TCPC(const std::string& name, uint16_t port) {
        NetworkAddress local_addr;

        _socket.set(::socket(AF_INET6, SOCK_STREAM, 0));
        if (_socket.invalid()) {
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
        }

        /*
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
            NetworkError(NetworkErrorCodeResolveAddress, iResult);
            return;
        }

        // Find potential addresses
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

        freeaddrinfo(result);

        if (_socket.invalid()) {
            NetworkError(NetworkErrorCodeConnect, 0);
            return;
        }*/
    }

    TCPC::TCPC(SOCKET client, NetworkAddress remote_addr) : SocketBase<TCPCHandle>(client), _remote_addr(remote_addr) {

    }

    bool TCPC::send(const Utils::ConstBlobView& data) {
        if (_socket.invalid()) return false;

        int progress = 0;

        while (progress < data.size()) {
            //Send bytes till all sent
            int iSendResult = ::send(_socket.get(), (char*)(data.getr() + progress), data.size() - progress, 0);

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

    bool TCPC::read(Utils::BlobView& data, uint32_t min_len, uint32_t max_len) {
        //Allow writing to the entire area
        //Pre-allocates buffer
        data.resize(max_len);
        
        int progress = 0;

        //Even if min 0 requested, still do one round
        do {
            Utils::BlobSliceView write_area = data.slice_right(progress);

            int recv_len = recv(_socket.get(), (char*)write_area.getw(), write_area.size(), 0);

            //Error
            if (recv_len == SOCKET_ERROR) {
                NetworkError(NetworkErrorCodeReciveData, WSAGetLastError());
                return false;
            }

            //Connection gracefully closed
            if (recv_len == 0) {
                return false;
            }

            progress += recv_len;
        } while (progress < min_len);

        data.resize(progress);

        return true;
    }

    const NetworkAddress& TCPC::get_remote_addr() {
        return _remote_addr;
    }

    TCPC::~TCPC() {
        _socket.close();
    }

    //
    // UDP
    //

    UDP::UDP(const std::string& name, uint16_t remote_port, uint16_t local_port) : UDP(NetworkAddress(name, remote_port), NetworkAddress("", local_port)) {

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
        iResult = ::bind(_socket.get(), (const sockaddr*)_local_address.get(), _local_address.size());
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeBindListenSocket, WSAGetLastError());
            _socket.close();
            return;
        }
    }

    bool UDP::send(const Utils::ConstBlobView& data) {
        // Send a datagram to the receiver
        int iResult = ::sendto(_socket.get(),
            (char*)data.getr(), data.size(), 0, (const sockaddr*)_remote_address.get(), _remote_address.size());
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeSendData, WSAGetLastError());
            _socket.close();
            return false;
        }
        return true;
    }

    bool UDP::readFilter(Utils::BlobView& data, uint32_t max_len) {
        NetworkAddress src;
        bool res = read(data, src, max_len);

        if (!res) return false;

        if (src == _remote_address) {
            return res;
        }
        return false;
    }

    bool UDP::read(Utils::BlobView& data, NetworkAddress& address, uint32_t max_len) {
        data.resize(max_len);

        int size = *address.sizew();
        int iResult = recvfrom(_socket.get(), (char*)data.getw(), data.size(), 0, (sockaddr*)address.getw(), &size);
        *address.sizew() = size;
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeReciveData, WSAGetLastError());
            _socket.close();
            return false;
        }

        data.resize(iResult);
        return true;
    }

    UDP::~UDP() {
        _socket.close();
    }

}

#endif