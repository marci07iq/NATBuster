#pragma once

#ifdef WIN32

#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#include "network.h"
#include "../utils/no_copy.h"

namespace NATBuster::Common::Network {
    enum NetworkErrorCodes {
        NetworkErrorCodeInitalize,
        NetworkErrorCodeResolveServerAddress,
        NetworkErrorCodeResolveClientAddress,
        NetworkErrorCodeCreateListenSocket,
        NetworkErrorCodeCreateServerSocket,
        NetworkErrorCodeCreateClientSocket,
        NetworkErrorCodeConnectServer,
        NetworkErrorCodeConnectClient,
        NetworkErrorCodeServerSendData,
        NetworkErrorCodeServerReciveData,
        NetworkErrorCodeClientSendData,
        NetworkErrorCodeClientReciveData,
        NetworkErrorCodeBindListenSocket,
        NetworkErrorCodeServerListen,
        NetworkErrorCodeServerAccept,
    };

    void NetworkError(int ID, int WSAError) {
        std::cerr << "Error code: " << ID << std::endl;
    }

    class SocketWrapper : Utils::NonCopyable {
        SOCKET _socket;
    public:
        SocketWrapper(SOCKET socket = INVALID_SOCKET) : _socket(socket) {

        }

        inline bool valid() {
            return _socket != INVALID_SOCKET;
        }

        inline bool invalid() {
            return _socket == INVALID_SOCKET;
        }

        inline void close() {
            if (valid()) {
                closesocket(_socket);
                _socket = INVALID_SOCKET;
            }
        }

        inline void set(SOCKET socket) {
            close();
            _socket = socket;
        }

        SOCKET get() {
            return _socket;
        }

        ~SocketWrapper() {
            if (valid()) {
                closesocket(_socket);
                _socket = INVALID_SOCKET;
            }
        }
    };

    class WSAContainer {
        WSADATA _wsa_data;
    public:

        WSAContainer(uint8_t major = 2, uint8_t minor = 2) {
            //Create an instance of winsock
            int iResult = WSAStartup(MAKEWORD(major, minor), &_wsa_data);
            if (iResult != 0) {
                NetworkError(NetworkErrorCodeInitalize, iResult);
                return;
            }
        }

        WSADATA get() const {
            return _wsa_data;
        }

        ~WSAContainer()  {
            WSACleanup();
        }
    };

    //
    // TCP Server OS implementation
    // 

    class TCPS {
        SocketWrapper _listen_socket;
    public:
        TCPS(std::string name, uint16_t port);

        TCPCHandle accept();

        static TCPSHandle findConnection(std::vector<TCPSHandle> sockets);

        ~TCPS();
    };

    //
    // TCP Client OS implementation
    //

    class TCPC {
        SocketWrapper _client_socket;
    public:
        TCPC(std::string name, uint16_t port);
        TCPC(SOCKET client);

        bool send(Packet data);

        bool isOpen();

        void close();

        Packet read();

        static TCPCHandle findReadable(std::vector<TCPCHandle> sockets);

        ~TCPC();
    };
}

#endif