#include "time.h"

namespace NATBuster::Time {
    const time_delta_type_us TIME_DELTA_INFINTE = -1;
    const time_delta_type_us TIME_DELTA_ZERO = 0;

    time_type_us now() {
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

        return start.time_since_epoch() /
            std::chrono::microseconds(1);
    }

    time_type_us earlier(time_type_us a, time_type_us b) {
        return (a < b) ? a : b;
    }
}