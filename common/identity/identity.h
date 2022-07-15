#pragma once

#include <list>
#include <vector>

#include "../crypto/pkey.h"
#include "../crypto/hash.h"

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
        std::string _name;
        std::shared_ptr<PermGroup> _base;
    public:
        PermGroup(const std::string& name, std::shared_ptr<PermGroup> base) : _name(name), _base(base) {

        }
    };

    class User : public PermGroup {
    public:
        static std::shared_ptr<User> Anonymous;

        const Crypto::PKey key;

        User(const std::string& name, Crypto::PKey&& key, std::shared_ptr<PermGroup> base = std::shared_ptr<PermGroup>()) :
            PermGroup(name, base), key(std::move(key)) {

        }

        bool isAnonymous() {
            return key.has_key();
        }
    };

    class UserGroup {
        std::list<std::shared_ptr<User>> _identites;
    public:
        //Find key among known identities
        std::shared_ptr<User> findUser(const Crypto::PKey& key) const {
            for (const auto& it : _identites) {
                if (it->key.is_same(key)) {
                    return it;
                }
            }
            return std::shared_ptr<User>();
        }

        //Get the user associate with a key
        //If key is not known, returns anonymous
        std::shared_ptr<User> findUserDefault(const Crypto::PKey& key) const {
            std::shared_ptr<User> known = findUser(key);
            if (known) {
                return known;
            }
            return User::Anonymous;
        }

        bool isMember(const Crypto::PKey& key) const {
            if (!key.has_key()) return false;
            for (const auto& it : _identites) {
                if (it->key.is_same(key)) {
                    return true;
                }
            }
            return false;
        }

        bool addUser(std::shared_ptr<User> identity) {
            //Anonymous key can't be added
            if (!identity->key.has_key()) {
                return false;
            }
            std::shared_ptr<User> known = findUser(identity->key);
            if (known) {
                return false;
            }
            else {
                _identites.push_back(identity);
                return true;
            }
        }
    };

}