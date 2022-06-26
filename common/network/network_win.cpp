#ifdef WIN32

#include "network_win.h"

namespace NATBuster::Common::Network {
    //Hold WSA instance
    const WSAContainer wsa(2, 2);

    #define BUFFER_LENGTH 5000

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
            NetworkError(NetworkErrorCodeResolveServerAddress, iResult);
            return;
        }

        // Create the server listen socket
        _listen_socket.set(::socket(result->ai_family, result->ai_socktype, result->ai_protocol));
        if (_listen_socket.invalid()) {
            NetworkError(NetworkErrorCodeCreateListenSocket, 0);
            freeaddrinfo(result);
            return;
        }

        // Bind the listen socket
        iResult = ::bind(_listen_socket.get(), result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeBindListenSocket, 0);
            freeaddrinfo(result);
            _listen_socket.close();
            return;
        }

        freeaddrinfo(result);

        // Set socket to listening mode
        iResult = ::listen(_listen_socket.get(), SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            NetworkError(NetworkErrorCodeServerListen, 0);
            _listen_socket.close();
            return;
        }
    }


    TCPCHandle TCPS::accept() {
        int iResult;
        SOCKET clientSocket;

        // Accept a client socket
        clientSocket = ::accept(_listen_socket.get(), NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            NetworkError(NetworkErrorCodeServerAccept, 0);
            _listen_socket.close();


            return std::shared_ptr<TCPC>();
        }

        return std::make_shared<TCPC>(clientSocket);
    }

    TCPSHandle TCPS::findConnection(std::vector<TCPSHandle> sockets) {
        FD_SET collection;

        FD_ZERO(&collection);

        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_listen_socket;

            if (sw.valid()) {
                FD_SET(sw.get(), &collection);
            }
        }

        int count = select(0, &collection, NULL, NULL, NULL);

        if (count == SOCKET_ERROR || count == 0) {
            return TCPSHandle();
        }

        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_listen_socket;

            if (sw.valid()) {
                if (FD_ISSET(socket->_listen_socket.get(), &collection)) {
                    return socket;
                }
            }
        }
        return TCPSHandle();
    }

    TCPS::~TCPS() {
        _listen_socket.close();
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
            NetworkError(NetworkErrorCodeResolveServerAddress, 0);
            return;
        }

        // Find potential addresses
        for (addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            // Create socket
            _client_socket.set(::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol));
            if (_client_socket.invalid()) {
                NetworkError(NetworkErrorCodeCreateClientSocket, 0);
                return;
            }

            // Connect to server.
            iResult = ::connect(_client_socket.get(), ptr->ai_addr, (int)ptr->ai_addrlen);
            if (iResult == SOCKET_ERROR) {
                _client_socket.close();
                continue;
            }
            break;
        }

        freeaddrinfo(result);

        if (_client_socket.invalid()) {
            NetworkError(NetworkErrorCodeConnectServer, 0);
            return;
        }
    }

    TCPC::TCPC(SOCKET client) : _client_socket(client) {

    }

    bool TCPC::send(Packet data) {
        if (_client_socket.invalid()) return false;

        int iSendResult = ::send(_client_socket.get(), (char*)(data.get()), data.size(), 0);

        if (iSendResult != data.size()) {
            NetworkError(NetworkErrorCodeServerSendData, WSAGetLastError());
            _client_socket.close();
            return false;
        }
        return true;
    }

    bool TCPC::isOpen() {
        return _client_socket.valid();
    }

    void TCPC::close() {
        _client_socket.close();
    }

    Packet TCPC::read(uint32_t max_len) {
        uint8_t* res = new uint8_t(max_len);

        int recv_len = recv(_client_socket.get(), (char*)res, max_len, 0);
        
        if (recv_len == 0) {
            return Packet();
        }

        return Packet(recv_len, res);

        delete res;
    }

    TCPCHandle TCPC::findReadable(std::vector<TCPCHandle> sockets) {
        FD_SET collection;

        FD_ZERO(&collection);

        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_client_socket;

            if (sw.valid()) {
                FD_SET(sw.get(), &collection);
            }
        }

        int count = select(0, &collection, NULL, NULL, NULL);

        if (count == SOCKET_ERROR || count == 0) {
            return TCPCHandle();
        }

        for (auto&& socket : sockets) {
            SocketWrapper& sw = socket->_client_socket;

            if (sw.valid()) {
                if (FD_ISSET(socket->_client_socket.get(), &collection)) {
                    return socket;
                }
            }
        }
        return TCPCHandle();
    }

}

#endif