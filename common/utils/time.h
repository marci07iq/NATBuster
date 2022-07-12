#pragma once

#include <chrono>
#include <stdint.h>

#include "../os.h"

namespace NATBuster::Common::Time
{
    typedef uint64_t time_type_us;
    typedef int64_t time_delta_type_us;
    extern const time_delta_type_us TIME_DELTA_INFINTE;
    extern const time_delta_type_us TIME_DELTA_ZERO;

    time_type_us now();

    time_type_us earlier(time_type_us a, time_type_us b);

#ifdef WIN32
    struct Timeout {
        const time_delta_type_us _timeout_us;

        const timeval _timeout_os;

        Timeout(time_delta_type_us timeout_us) : _timeout_us(timeout_us), _timeout_os({
                .tv_sec = long(_timeout_us / 1000000),
                .tv_usec = long(_timeout_us % 1000000)
            }) {
        }

        inline bool infinite() {
            return _timeout_us < 0;
        }

        inline const timeval* to_native() {
            return infinite() ? nullptr : (&_timeout_os);
        }
    };
#endif
}