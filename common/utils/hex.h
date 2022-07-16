#pragma once

#include <iostream>
#include <string>

#include "blob.h"

namespace NATBuster::Common::Utils {
    //Set sep = '\0' to disable separator.
    void print_hex(const ConstBlobView& data, std::ostream& dst = std::cout, char sep = ':');
    void to_hex(const ConstBlobView& data, BlobView& dst, char sep = ':');
};