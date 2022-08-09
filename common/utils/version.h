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
            } field;

        };
        
        std::string to_string() const {
            return
                std::to_string(field.major) + "." +
                std::to_string(field.minor) + "." +
                std::to_string(field.build);
        }
    };


    static_assert(offsetof(version, num) == 0);
    static_assert(offsetof(version, field.major) == 0);
    static_assert(offsetof(version, field.minor) == 1);
    static_assert(offsetof(version, field.build) == 2);

#define VERSION_NUMBER_MAJOR 1
#define VERSION_NUMBER_MINOR 0
#define VERSION_NUMBER_BUILD 0

    std::ostream& operator<<(std::ostream& of, const version& version);

    extern const version current_version;
}