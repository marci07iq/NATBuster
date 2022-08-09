#include "version.h"

namespace NATBuster::Utils {
    version::version(uint32_t num_) : num(num_) {

    }

    version::version(uint8_t major_, uint8_t minor_, uint16_t build_) {
        field.major = major_;
        field.minor = minor_;
        field.build = build_;
    }

    std::ostream& operator<<(std::ostream& of, const version& version) {
        of << version.to_string();
        return of;
    }

    const version current_version(
        VERSION_NUMBER_MAJOR,
        VERSION_NUMBER_MINOR,
        VERSION_NUMBER_BUILD);
}