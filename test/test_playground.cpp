#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>

#include <thread>
#include <iostream>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DATA_BUFSIZE 4096

void apc_fun(ULONG_PTR) {
    std::cout << "APC @ " << std::this_thread::get_id() << std::endl;
}

void remote_wake(HANDLE to_wake) {
    int i;
    do {
        std::cout << "Waker @ " << std::this_thread::get_id() << std::endl;
        std::cout << "Wake?" << std::endl;
        std::cin >> i;
        QueueUserAPC(apc_fun, to_wake, 0);
    } while (i == 1);
}

int main()
{
    //-----------------------------------------
    // Declare and initialize variables
    WSADATA wsaData = { 0 };
    int iResult = 0;
    BOOL bResult = TRUE;

    WSABUF DataBuf;
    char buffer[DATA_BUFSIZE];

    DWORD EventTotal = 0;
    DWORD RecvBytes = 0;
    DWORD Flags = 0;
    DWORD BytesTransferred = 0;

    WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];
    WSAOVERLAPPED AcceptOverlapped;
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET AcceptSocket = INVALID_SOCKET;

    DWORD Index;

    //-----------------------------------------
    // Initialize Winsock
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        wprintf(L"WSAStartup failed: %d\n", iResult);
        return 1;
    }
    //-----------------------------------------
    // Create a listening socket bound to a local
    // IP address and the port specified
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        wprintf(L"socket failed with error = %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    u_short port = 27015;
    char* ip;
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(port);
    hostent* thisHost;

    thisHost = gethostbyname("");
    if (thisHost == NULL) {
        wprintf(L"gethostbyname failed with error = %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    ip = inet_ntoa(*(struct in_addr*)*thisHost->h_addr_list);

    std::cout << ip << std::endl;

    service.sin_addr.s_addr = inet_addr(ip);

    //-----------------------------------------
    // Bind the listening socket to the local IP address
    // and port number
    iResult = bind(ListenSocket, (SOCKADDR*)&service, sizeof(SOCKADDR));
    if (iResult != 0) {
        wprintf(L"bind failed with error = %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    //-----------------------------------------
    // Set the socket to listen for incoming
    // connection requests
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult != 0) {
        wprintf(L"listen failed with error = %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    wprintf(L"Listening...\n");

    //-----------------------------------------
    // Accept and incoming connection request
    AcceptSocket = accept(ListenSocket, NULL, NULL);
    if (AcceptSocket == INVALID_SOCKET) {
        wprintf(L"accept failed with error = %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    wprintf(L"Client Accepted...\n");

    //-----------------------------------------
    // Create an event handle and setup an overlapped structure.
    EventArray[EventTotal] = WSACreateEvent();
    if (EventArray[EventTotal] == WSA_INVALID_EVENT) {
        wprintf(L"WSACreateEvent failed with error = %d\n", WSAGetLastError());
        closesocket(AcceptSocket);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    WSAEventSelect(AcceptSocket, EventArray[EventTotal], FD_READ | FD_CLOSE);

    /*ZeroMemory(&AcceptOverlapped, sizeof(WSAOVERLAPPED));
    AcceptOverlapped.hEvent = EventArray[EventTotal];*/

    DataBuf.len = DATA_BUFSIZE;
    DataBuf.buf = buffer;

    EventTotal++;

    std::cout << "Prepare for waiting @ " << std::this_thread::get_id() << std::endl;

    //Start waker thread, with a HANDLE to this
    HANDLE hThisThread;
    DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentThread(),
        GetCurrentProcess(),
        &hThisThread,
        0,
        TRUE,
        DUPLICATE_SAME_ACCESS);
    std::thread waker(remote_wake, hThisThread);

    //-----------------------------------------
    // Process overlapped receives on the socket
    while (1) {

        //-----------------------------------------
        // Call WSARecv to receive data into DataBuf on 
        // the accepted socket in overlapped I/O mode
        /*if (WSARecv(AcceptSocket, &DataBuf, 1, &RecvBytes, &Flags, &AcceptOverlapped, NULL) ==
            SOCKET_ERROR) {
            iResult = WSAGetLastError();
            if (iResult != WSA_IO_PENDING)
                wprintf(L"WSARecv failed with error = %d\n", iResult);
        }*/


        //Index = WaitForMultipleObjectsEx(EventTotal, EventArray, FALSE, 10000, TRUE);

        //-----------------------------------------
        // Wait for the overlapped I/O call to complete
        Index = WSAWaitForMultipleEvents(EventTotal, EventArray, FALSE, 10000, TRUE);

        std::cout << "Woken @ " << std::this_thread::get_id() << std::endl;
        if (Index == WAIT_IO_COMPLETION) {
            std::cout << "APC" << std::endl;
        }
        else if (Index == WAIT_TIMEOUT) {
            std::cout << "TIMEOUT" << std::endl;
        }
        else if (Index == WAIT_OBJECT_0) {
            std::cout << "EVENT" << std::endl;

            WSANETWORKEVENTS NetworkEvents;
            WSAEnumNetworkEvents(AcceptSocket, EventArray[0], &NetworkEvents);

            //-----------------------------------------
            // Reset the signaled event
            bResult = WSAResetEvent(EventArray[Index - WAIT_OBJECT_0]);
            if (bResult == FALSE) {
                wprintf(L"WSAResetEvent failed with error = %d\n", WSAGetLastError());
            }
            
            if (NetworkEvents.lNetworkEvents & FD_READ) {
                int recvd = recv(AcceptSocket, buffer, 4096, 0);
                std::cout << "READ " << recvd << std::endl;
                if (recvd == 0) {
                    closesocket(AcceptSocket);
                    break;
                }
                else {
                    send(AcceptSocket, buffer, recvd, 0);
                }
            }

            if (NetworkEvents.lNetworkEvents & FD_CLOSE) {
                std::cout << "CLOSE " << std::endl;
                closesocket(AcceptSocket);
                break;
            }
        }
        else {
            std::cout << "UNKNOWN " << Index << std::endl;
        }
    }

    WSACloseEvent(EventArray[0]);

    std::cout << "Done, please join thread" << std::endl;
    waker.join();

    closesocket(ListenSocket);
    closesocket(AcceptSocket);
    WSACleanup();

    return 0;
}

