#ifdef WIN32
#include <json/json.h>
#else
#include <jsoncpp/json/json.h>
#endif

#include "../identity/identity.h"

namespace NATBuster::Config {
    std::unique_ptr<Identity::User> read_user(const Json::Value& node);
    std::unique_ptr<Identity::User> write_user(const Json::Value& node);

    std::unique_ptr<Identity::UserGroup> read_user_group(const Json::Value& node);
    std::unique_ptr<Identity::UserGroup> write_user_group(const Json::Value& node);
}