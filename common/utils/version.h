#pragma once

#include "stdint.h"
#include <iostream>
#include <string>

namespace NATBuster::Utils {
    struct version {
        version(uint32_t num);
        version(uint8_t major, uint8_t minor, uint16_t build);
        union {
            uint32_t num;
            struct {
                uint8_t major;
                uint8_t minor;
                uint16_t build;
            };

        };
        
        std::string to_string() const {
            return
                std::to_string(major) + "." +
                std::to_string(minor) + "." +
                std::to_string(build);
        }
    };
    static_assert(offsetof(version, num) == 0);
    static_assert(offsetof(version, major) == 0);
    static_assert(offsetof(version, minor) == 1);
    static_assert(offsetof(version, build) == 2);


    std::ostream& operator<<(std::ostream& of, const version& version);

    extern const version current_version;
}