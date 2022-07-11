#include "time.h"

namespace NATBuster::Common::Time {
    const time_delta_type_us TIME_DELTA_INFINTE = -1;
    const time_delta_type_us TIME_DELTA_ZERO = 0;

    time_type_us now() {
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

        return start.time_since_epoch() /
            std::chrono::microseconds(1);
    }
}