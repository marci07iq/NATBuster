#pragma once

#include <functional>

#include "../utils/blob.h"
#include "../utils/callbacks.h"
#include "../utils/copy_protection.h"
#include "../utils/event.h"

namespace NATBuster::Common::Transport {
    //Ordered packet transport
    class OPTBase : public Utils::EventEmitter<const Utils::ConstBlobView&> {
    public:
        //using PacketCallback = Utils::Callback<const Utils::ConstBlobView&>;
        //using RawCallback = Utils::Callback<const Utils::ConstBlobView&>;
        //using ErrorCallback = Utils::Callback<>;
        //using CloseCallback = Utils::Callback<>;

    protected:
        ResultCallback _raw_callback;
        //PacketCallback _packet_callback = nullptr;
        //RawCallback _raw_callback = nullptr;
        //ErrorCallback _error_callback = nullptr;
        //CloseCallback _close_callback = nullptr;

        OPTBase();
    private:
        using Utils::EventEmitter<const Utils::ConstBlobView&>::set_result_callback;
    public:
        inline void set_packet_callback(
            ResultCallback::raw_type packet_callback) {
            set_result_callback(packet_callback);
        }

        void set_raw_callback(
            ResultCallback::raw_type raw_callback) {
            _raw_callback = raw_callback;
        }

        //Send ordered packet
        virtual void send(const Utils::ConstBlobView& packet) = 0;
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        virtual void sendRaw(const Utils::ConstBlobView& packet) = 0;
        //Close connection (gracefully) if possible
        virtual void close() = 0;
    };
};