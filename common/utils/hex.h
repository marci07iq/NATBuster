#pragma once

#include <iostream>
#include <string>

#include "blob.h"

namespace NATBuster::Common::Utils {
    void print_hex(const ConstBlobView& data, std::ostream& dst = std::cout, char sep = ':');
};