#include <chrono>
#include <stdint.h>

#ifdef WIN32
#include "windows.h"
#endif

namespace NATBuster::Common::Time
{
    typedef uint64_t time_type_us;

    time_type_us now() {
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

        return start.time_since_epoch() /
            std::chrono::microseconds(1);
    }

#ifdef WIN32
    struct Timeout {
        static const int64_t INFINTE = -1;
        static const int64_t ZERO = 0;

        const int64_t _timeout_us;

        const timeval _timeout_os;

        Timeout(int64_t timeout_us) : _timeout_us(timeout_us), _timeout_os({
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