#include "hex.h"

namespace NATBuster::Common::Utils {
    void print_hex(const ConstBlobView& data, std::ostream& dst, char sep) {
        const char hex_chars[] = "0123456789ABCDEF";
        for (uint32_t i = 0; i < data.size(); i++) {
            if ((0 < i) && (sep != 0)) {
                dst << sep;
            }
            dst << hex_chars[data.getr()[i] >> 4] << hex_chars[data.getr()[i] & 0xf];
        }
    }
};