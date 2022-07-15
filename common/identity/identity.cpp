#include "identity.h"

namespace NATBuster::Common::Identity {
    std::shared_ptr<User> User::Anonymous = std::make_shared<User>("anonymous", Crypto::PKey());
};