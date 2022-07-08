#pragma once

#include <cassert>
#include <cstdint>
#include <array>

#include <openssl/evp.h>

#include "../crypto/cipher.h"

#include "../utils/copy_protection.h"
#include "../utils/blob.h"
#include "../utils/time.h"

namespace NATBuster::Common::Proto {
    enum class KEX_Event {
        //KEX error. terminate connection

        //Unknown error
        ErrorGeneric = 0,
        //Malformed packet received
        ErrorMalformed = 1,
        //Remote public key is not as expected
        ErrorNotrust = 2,
        //Failed: Crypto error occured
        ErrorFailed = 3,

        //Valid KEX message received
        OK = 16,
        //New keys are ready to use. All subsequent receive should be decoded with the new key
        OK_Newkey = 17,
        //KEX and auth are ready. The tunnle is open for business
        OK_Ready = 18,
    };

    class KEX : Utils::NonCopyable {
    protected:
        KEX() {

        }

        Time::time_type_us _last_kex;
    public:
        inline Time::time_type_us last_kex() {
            return _last_kex;
        }

        virtual KEX_Event recv(const Utils::ConstBlobView& packet) = 0;

        //To be called after an OK_Newkey event
        virtual void set_key(Crypto::CipherPacketStream& stream) = 0;


    };

};