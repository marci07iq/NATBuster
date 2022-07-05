#include "hex.h"

namespace NATBuster::Common::Utils {
    void print_hex(const BlobView& data, std::ostream& dst, char sep) {
        const char hex_chars[] = "0123456789ABCDEF";
        for (int i = 0; i < data.size(); i++) {
            if ((0 < i) && (sep != 0)) {
                dst << sep;
            }
            dst << hex_chars[data.get()[i] >> 4] << hex_chars[data.get()[i] & 0xf];
        }
    }
};