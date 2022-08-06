#pragma once

#include <list>
#include <vector>

#include "../crypto/pkey.h"
#include "../crypto/hash.h"
#include "../utils/hex.h"

namespace NATBuster::Common::Identity {
    /*enum SettingOverride : int8_t {
        //Override previous value to allow
        Allow_Override = 1,
        //Leave as-is
        Default = 0,
        //Override to deny
        Deny_Override = -1,
    };

    //Fallback is used if the enum is invalid
    bool updateSetting(const bool setting, const SettingOverride& next, bool fallback = false) {
        switch (next)
        {
        case Allow_Override:
            return true;
        case Default:
            return setting;
        case Deny_Override:
            return false;
        default:
            assert(false);
            return fallback;
            break;
        }
    }*/

    struct Permissions {
        //Ports are always additive
        struct PortSettings {
            //Which way can the port be forwarded

            bool dir_pull : 1;
            bool dir_push : 1;

            //Which protocol

            bool proto_tcp : 1;
            bool proto_udp : 1;

            //Allow users to provide a range of ports

            //Inclusive
            uint16_t port_min;
            //Inclusive
            uint16_t port_max;
        };
        std::vector<PortSettings> ports;
    };

    class PermGroup : public Permissions {
        std::shared_ptr<PermGroup> _base;
    public:
        std::string name;
        PermGroup(const std::string& name, std::shared_ptr<PermGroup> base) : name(name), _base(base) {

        }
    };

    class User : public PermGroup {
    public:
        static std::shared_ptr<User> Anonymous;

        const std::shared_ptr<const Crypto::PuKey> key;

        User(
            const std::string& name,
            const std::shared_ptr<const Crypto::PuKey> key,
            std::shared_ptr<PermGroup> base = std::shared_ptr<PermGroup>()) :
            PermGroup(name, base), key(key) {

        }

        bool isAnonymous() {
            return key->has_key();
        }
    };

    std::ostream& operator<<(std::ostream& os, const User& user);

    class UserGroup {
        Utils::BlobIndexedMap<std::shared_ptr<User>> _identites;
    public:
        //Find fingerprint among known identities
        std::shared_ptr<User> findUser(const Utils::ConstBlobView& key) const {
            auto it = _identites.find(key);
            if (it != _identites.end()) {
                return it->second;
            }
            return std::shared_ptr<User>();
        }

        //Find key among known identities
        std::shared_ptr<User> findUser(const Crypto::PuKey& key) const {
            Utils::Blob fingerprint;
            key.fingerprint(fingerprint);
            return findUser(fingerprint);
        }

        //Get the user associate with a key
        //If key is not known, returns anonymous
        std::shared_ptr<User> findUserDefault(const Utils::ConstBlobView& fingerprint) const {
            std::shared_ptr<User> known = findUser(fingerprint);
            if (known) {
                return known;
            }
            return User::Anonymous;
        }

        //Get the user associate with a key
        //If key is not known, returns anonymous
        std::shared_ptr<User> findUserDefault(const Crypto::PuKey& key) const {
            std::shared_ptr<User> known = findUser(key);
            if (known) {
                return known;
            }
            return User::Anonymous;
        }

        bool isMember(const Crypto::PuKey& key) const {
            if (!key.has_key()) return false;
            for (const auto& it : _identites) {
                if (it.second->key->is_same(key)) {
                    return true;
                }
            }
            return false;
        }

        bool addUser(std::shared_ptr<User> identity) {
            //Anonymous key can't be added
            if (!identity->key->has_key()) {
                return false;
            }
            Utils::Blob fingerprint;
            identity->key->fingerprint(fingerprint);
            std::shared_ptr<User> known = findUser(fingerprint);
            if (known) {
                return false;
            }
            else {
                _identites.emplace(std::move(fingerprint), identity);
                return true;
            }
        }
    };

}