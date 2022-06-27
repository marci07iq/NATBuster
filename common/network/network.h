#pragma once

#include <memory>
#include <string>
#include <stdint.h>

#include "../utils/data_queue.h"

namespace NATBuster::Common::Network {
    class TCPS;
    class TCPC;
    class UDP;

    typedef std::shared_ptr<TCPS> TCPSHandle;
    typedef std::shared_ptr<TCPC> TCPCHandle;
    typedef std::shared_ptr<UDP> UDPHandle;

    enum NetworkErrorCodes {
        //Initilaise network subsystem
        NetworkErrorCodeInitalize,

        //Resolve listen address
        NetworkErrorCodeResolveAddress,

        //Create listening socket
        NetworkErrorCodeCreateListenSocket,
        //Bind listening socket
        NetworkErrorCodeBindListenSocket,
        //Listen
        NetworkErrorCodeListen,
        //Accept
        NetworkErrorCodeAccept,

        //Create client socket
        NetworkErrorCodeCreateClientSocket,
        //Connect to server
        NetworkErrorCodeConnect,

        //Send function
        NetworkErrorCodeSendData,
        //Listen function
        NetworkErrorCodeReciveData,

        //Select acceptable socket
        NetworkErrorCodeSelectAccept,
        //Select readable socket
        NetworkErrorCodeSelectRead,

    };

    class Packet {
        std::shared_ptr<uint8_t[]> _data;
        uint32_t _length;

        Packet(uint32_t length, uint8_t* consume_data);
    public:
        //Empty
        Packet();

        static Packet copy_from(uint32_t length, uint8_t* copy_data);
        static Packet copy_from(const Packet& packet);
        static Packet consume_from(uint32_t length, uint8_t* consume_data);


        //Packet(const Packet& other);

        inline uint32_t size() const {
            return _length;
        }

        inline uint8_t* get() const {
            return _data.get();
        }
    };
};

#include "network_win.h"