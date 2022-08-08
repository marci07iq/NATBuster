#include "identity.h"

namespace NATBuster::Identity {
    std::shared_ptr<User> User::Anonymous = std::make_shared<User>("anonymous", std::make_shared<Crypto::PrKey>());

    std::ostream& operator<<(std::ostream& os, const User& user)
    {
        Utils::Blob fingerprint;
        user.key->fingerprint(fingerprint);

        os << user.name << "[";
        Utils::print_hex(fingerprint.cslice_left(8), os);
        os << "...]";
        return os;
    }
};