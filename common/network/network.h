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

    class Packet {
        std::shared_ptr<uint8_t[]> _data;
        uint32_t _length;

    public:
        //Empty
        Packet();

        //Data is copied. Caller must destroy data after.
        Packet(uint32_t length, uint8_t* copy_data = nullptr);

        Packet(const Packet& other);

        inline uint32_t size() const {
            return _length;
        }

        inline uint8_t* get() const {
            return _data.get();
        }
    };
};

#include "network_win.h"