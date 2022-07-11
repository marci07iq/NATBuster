#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include "../utils/copy_protection.h"
#include "../utils/blob.h"
#include "../utils/time.h"

#include "../crypto/cipher.h"

namespace NATBuster::Common::Transport {
    class Session;
};

namespace NATBuster::Common::Proto {
    class KEX : Utils::NonCopyable {
    public:
        enum class KEX_Event {
            //KEX error. terminate connection

            //Unknown error
            ErrorGeneric = 0,
            //Malformed packet received
            ErrorMalformed = 1,
            //No trust in remote identity
            ErrorNotrust = 2,
            //Crypto error occured
            ErrorCrypto = 3,
            //Outbound packet couldnt be sent
            ErrorSend = 4,
            //State error
            ErrorState = 5,

            //Valid KEX message received
            OK = 16,
            //KEX and auth are ready. The tunnle is open for business
            OK_Done = 20,
        };


    protected:
        KEX() {

        }

        Time::time_type_us _last_kex;


    public:
        inline Time::time_type_us last_kex() {
            return _last_kex;
        }

        virtual KEX_Event recv(const Utils::ConstBlobView& packet, Transport::Session* out) = 0;

        virtual KEX_Event init_kex(Transport::Session* out) = 0;

        virtual ~KEX() {

        }
    };

};