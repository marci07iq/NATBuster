#include "version.h"

namespace NATBuster::Utils {
    version::version(uint32_t num_) : num(num_) {

    }

    version::version(uint8_t major_, uint8_t minor_, uint16_t build_) : major(major_), minor(minor_), build(build_) {

    }

    std::ostream& operator<<(std::ostream& of, const version& version) {
        of << version.to_string();
    }

    const version current_version(1, 0, 0);
}