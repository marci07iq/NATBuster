#pragma once

#ifdef WIN32
#include <json/json.h>
#else
#include <jsoncpp/json/json.h>
#endif


namespace NATBuster::Config {
    struct parse_status {
        bool ok;
        std::string message;

        inline parse_status prefix(const std::string& pref = "") {
            if (pref.empty()) return *this;
            return parse_status{
                .ok = ok,
                .message = pref + message
            };
        }

        inline static parse_status success() {
            return parse_status{
                .ok = true,
                .message = ""
            };
        };

        inline static parse_status error(const std::string& message) {
            return parse_status{
                .ok = false,
                .message = message
            };
        };

        inline operator bool() const {
            return ok;
        }

        inline bool operator!() const {
            return !ok;
        }
    };
}