#pragma once

#include <iostream>
#include <string>

#include "blob.h"

namespace NATBuster::Common::Utils {
    void print_hex(const BlobView& data, std::ostream& dst = std::cout, char sep = ':');
};