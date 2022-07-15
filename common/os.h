#pragma once

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

//#define FD_SETSIZE 512
#include <winsock2.h>
#include <mswsock.h>   // Need for SO_UPDATE_CONNECT_CONTEXT
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "Ws2_32.lib")
#endif