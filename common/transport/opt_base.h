#pragma once

#include <functional>

#include "../utils/blob.h"
#include "../utils/callbacks.h"
#include "../utils/copy_protection.h"
#include "../utils/event.h"

#include "../identity/identity.h"

namespace NATBuster::Common::Transport {
    //Ordered packet transport
    class OPTBase : public Utils::EventEmitter<const Utils::ConstBlobView&> {
    public:
        
    protected:
        ResultCallback _raw_callback;
        
        bool _is_client;

        OPTBase(bool is_client);
    private:
        using Utils::EventEmitter<const Utils::ConstBlobView&>::set_result_callback;
    public:
        //Called when a TCP type packet is available
        //All callbacks are issued from the same thread
        //Safe to call from any thread, even during a callback
        inline void set_packet_callback(
            ResultCallback::raw_type packet_callback) {
            set_result_callback(packet_callback);
        }

        //Called when a UDP type packet is available
        //All callbacks are issued from the same thread
        //Safe to call from any thread, even during a callback
        inline void set_raw_callback(
            ResultCallback::raw_type raw_callback) {
            _raw_callback = raw_callback;
        }

        //Send ordered packet
        virtual void send(const Utils::ConstBlobView& packet) = 0;
        //Send UDP-like packet (no order / arrival guarantee, likely faster)
        virtual void sendRaw(const Utils::ConstBlobView& packet) = 0;
        //Close connection (gracefully) if possible
        virtual void close() = 0;

        virtual std::shared_ptr<Identity::User> getUser() {
            return Identity::User::Anonymous;
        }
    };
};