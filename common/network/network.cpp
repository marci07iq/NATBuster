#include "network.h"


namespace NATBuster::Common::Network {

    Packet::Packet() : _length(0), _data() {
    }

    //Data is copied. Caller must destroy data after.
    Packet::Packet(uint32_t length, uint8_t* copy_data = nullptr) : _length(length) {
        uint8_t* new_data = new uint8_t(length);

        memcpy(new_data, copy_data, length);

        _data = std::shared_ptr<uint8_t[]>(new_data);
    }

    Packet::Packet(const Packet& other) : Packet(other.size(), other._data.get()) {

    }

}