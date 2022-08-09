#include "codes.h"

#include "../cli/color.h"

#include <string>
#include <map>

std::ostream& operator<<(std::ostream& stream, const NATBuster::ErrorCode& code) {
    stream << NATBuster::ErrorCodes::description(code);
    return stream;
}

NATBuster::ErrorCode operator&(const NATBuster::ErrorCode& lhs, const NATBuster::ErrorCode& rhs) {
    return (NATBuster::ErrorCode)((uint32_t)lhs & (uint32_t)rhs);
}

namespace NATBuster::ErrorCodes {
    const std::map<ErrorCode, std::string> code_type_lookup = {
        {ErrorCode::TYPE_FATAL, "Fatal"},
        {ErrorCode::TYPE_ERROR, "Error"},
        {ErrorCode::TYPE_WARNING, "Warning"},
        {ErrorCode::TYPE_INFO, "Info"},
        {ErrorCode::TYPE_DEBUG, "Debug"},
    };

    const std::map<ErrorCode, std::string> code_type_color_lookup = {
        {ErrorCode::TYPE_FATAL, CLI::Color::BrightRed},
        {ErrorCode::TYPE_ERROR, CLI::Color::Red},
        {ErrorCode::TYPE_WARNING, CLI::Color::Yellow},
        {ErrorCode::TYPE_INFO, CLI::Color::White},
        {ErrorCode::TYPE_DEBUG, CLI::Color::Blue},
    };

    const std::map<ErrorCode, std::string> code_lookup = {
        {ErrorCode::GENERIC_FATAL_INVALID_VF, "Invalid virtual function"},

        {ErrorCode::NETWORK_ERROR_SYSTEM, "Network system failed"},
        {ErrorCode::NETWORK_ERROR_RESOLVE_HOSTNAME, "Can't resolve hostname"},
        {ErrorCode::NETWORK_ERROR_CREATE_SOCKET, "Can't create socket"},
        {ErrorCode::NETWORK_ERROR_SERVER_BIND, "Can't bind socket"},
        {ErrorCode::NETWORK_ERROR_SERVER_LISTEN, "Can't listen socket"},
        {ErrorCode::NETWORK_ERROR_CONNECT, "Can't connect"},
        {ErrorCode::NETWORK_WARN_CONNECTING, "Connection in progress"},

        {ErrorCode::OPT_UDP_INVALID_PING, "Invalid ping received"},
        {ErrorCode::OPT_UDP_INVALID_PONG, "Invalid pong received"},
        {ErrorCode::OPT_RX_OVERFLOW, "Receive buffer overflow"},

        {ErrorCode::CONFIG_INVALID_PKEY, "Invalid user key"},
    };

    std::string description(ErrorCode code) {
        ErrorCode type = code & ErrorCode::MASK_TYPE;

        auto type_it = code_type_lookup.find(type);
        std::string type_str = (type_it != code_type_lookup.end()) ? type_it->second : "Unknown";

        auto code_it = code_lookup.find(code);
        std::string code_str = (code_it != code_lookup.end()) ? code_it->second : ("Unknown(" + std::to_string((uint32_t)code) + ")");

        return "[" + type_str + "] " + code_str;
    }

    bool is_error(ErrorCode code) {
        return (code & ErrorCode::MASK_TYPE) == ErrorCode::TYPE_ERROR;
    }

    void ThrowError(ErrorCode code) {
        std::cerr << code << std::endl;
        throw code;
    };
}