#pragma once

#include <chrono>
#include <stdint.h>

namespace NATBuster::Time
{
    typedef uint64_t time_type_us;
    typedef int64_t time_delta_type_us;
    extern const time_delta_type_us TIME_DELTA_INFINTE;
    extern const time_delta_type_us TIME_DELTA_ZERO;

    time_type_us now();

    time_type_us earlier(time_type_us a, time_type_us b);

    struct Timeout {
        const time_delta_type_us _timeout_us;

        Timeout(time_delta_type_us timeout_us) : _timeout_us(timeout_us) {
        }

        inline bool infinite() {
            return _timeout_us < 0;
        }
    };
}