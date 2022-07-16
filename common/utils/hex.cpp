#include <cassert>

#include "hex.h"

namespace NATBuster::Common::Utils {
    const char hex_chars[] = "0123456789ABCDEF";

    void print_hex(const ConstBlobView& data, std::ostream& dst, char sep) {
        const uint8_t* src_arr = data.getr();
        for (uint32_t i = 0; i < data.size(); i++) {
            if ((0 < i) && (sep != 0)) {
                dst << sep;
            }
            dst << hex_chars[src_arr[i] >> 4] << hex_chars[src_arr[i] & 0xf];
        }
    }

    void to_hex(const ConstBlobView& data, BlobView& dst, char sep) {
        if (sep != 0) {
            dst.resize(data.size() * 3 - 1);
        }
        else {
            dst.resize(data.size() * 2);
        }
        const uint8_t* src_arr = data.getr();
        char* dst_arr = (char*)dst.getw();
        for (uint32_t i = 0, j = 0; i < data.size(); i++) {
            if ((0 < i) && (sep != 0)) {
                dst_arr[j++] = sep;
            }
            dst_arr[j++] = hex_chars[src_arr[i] >> 4];
            dst_arr[j++] = hex_chars[src_arr[i] & 0xf];
            assert(j <= data.size());
        }
    }
};