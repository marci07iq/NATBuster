#include "network.h"


namespace NATBuster::Common::Network {

    /*Packet::Packet() : _length(0), _data() {
    }

    //Data is copied. Caller must destroy data after.
    Packet::Packet(uint32_t length, uint8_t* consume_data = nullptr) : _length(length) {
        _data = std::shared_ptr<uint8_t[]>(consume_data);
    }

    Packet Packet::copy_from(uint32_t length, uint8_t* copy_data) {
        uint8_t* new_data = new uint8_t(length);

        memcpy(new_data, copy_data, length);

        return Packet(length, copy_data);
    }

    Packet Packet::copy_from(const Packet& packet) {
        return Packet::copy_from(packet.size(), packet.get());
    }

    Packet Packet::consume_from(uint32_t length, uint8_t* consume_data) {
        return Packet(length, consume_data);
    }

    Packet Packet::create_empty(uint32_t length) {
        uint8_t* mem = new uint8_t[length];
        memset(mem, 0, length);
        return Packet::consume_from(length, mem);
    }*/

    /*Packet::Packet(const Packet& other) : _length(other.size()) {
        uint8_t* new_data = new uint8_t(other.size());

        memcpy(new_data, other.get(), other.size());

        _data = std::shared_ptr<uint8_t[]>(new_data);
    }*/
}