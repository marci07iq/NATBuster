#include "../utils/text.h"
#include "identity.h"

#ifdef WIN32
#include <Windows.h>
#include <Lmcons.h>
#elif __linux__
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#else
#error "Unsupported OS"
#endif

namespace NATBuster::Identity {
    std::shared_ptr<User> User::Anonymous = std::make_shared<User>("anonymous", std::make_shared<Crypto::PrKey>());

    std::ostream& operator<<(std::ostream& os, const User& user)
    {
        Utils::Blob fingerprint;
        user.get_key()->fingerprint(fingerprint);

        os << user.get_name() << "[";
        Utils::print_hex(fingerprint.cslice_left(8), os);
        os << "...]";
        return os;
    }

    void Permissions::store(Json::Value& node) const {
        assert(node.empty());
        node.clear();
        auto ports_val = Json::Value(Json::arrayValue);
        for (auto&& it : ports) {
            Json::Value it_val;
            it_val["pull"] = it.dir_pull;
            it_val["push"] = it.dir_push;
            it_val["tcp"] = it.proto_tcp;
            it_val["udp"] = it.proto_udp;
            if (it.port_min == it.port_max) {
                it_val["port_min"] = it.port_min;
                it_val["port_max"] = it.port_max;
            }
            else {
                it_val["port"] = it.port_min;
            }
            ports_val.append(it_val);
        }
        node["ports"] = ports_val;
    }
    Config::parse_status Permissions::parse(const Json::Value& node) {
        Json::Value field;

        if (node.isMember("ports") && (field = node["ports"]).isArray()) {
            assert(ports.empty());
            ports.clear();
            for (int i = 0; i < (int)field.size(); i++) {
                Json::Value nodei = node[i];

                PortSettings settings;
                Json::Value field2;

                settings.dir_pull = false;
                if (nodei.isMember("pull")) {
                    if ((field2 = nodei["pull"]).isBool()) {
                        settings.dir_pull = field2.asBool();
                    }
                    else {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "].pull: Expected bool field");
                    }
                }

                settings.dir_push = false;
                if (nodei.isMember("push")) {
                    if ((field2 = nodei["push"]).isBool()) {
                        settings.dir_push = field2.asBool();
                    }
                    else {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "].push: Expected bool field");
                    }
                }

                settings.proto_tcp = false;
                if (nodei.isMember("tcp")) {
                    if ((field2 = nodei["tcp"]).isBool()) {
                        settings.proto_tcp = field2.asBool();
                    }
                    else {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "].tcp: Expected bool field");
                    }
                }

                settings.proto_udp = false;
                if (nodei.isMember("udp")) {
                    if ((field2 = nodei["udp"]).isBool()) {
                        settings.proto_udp = field2.asBool();
                    }
                    else {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "].udp: Expected bool field");
                    }
                }

                if (nodei.isMember("port")) {
                    if (nodei.isMember("port_min")) {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "].port: Conflicting field port_min");
                    }
                    if (nodei.isMember("port_max")) {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "].port: Conflicting field port_max");
                    }

                    if (nodei["port"].isIntegral()) {
                        uint64_t port_res = field2.asUInt64();
                        if (0 < port_res && port_res < 65536) {
                            settings.port_max = settings.port_min = (uint16_t)port_res;
                        }
                        else {
                            return Config::parse_status::error(".ports[" + std::to_string(i) + "].port: Port out of range");
                        }
                    }
                    else {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "].port: Expected integer field port, or fields port_min and port_max");
                    }
                }
                else {
                    Json::Value field3;

                    if (node.isMember("port_min") && (field2 = node["port_min"]).isIntegral() &&
                        node.isMember("port_max") && (field3 = node["port_max"]).isIntegral()) {
                        uint64_t port_res = field2.asUInt64();
                        if (0 < port_res && port_res < 65536) {
                            settings.port_min = (uint16_t)port_res;
                        }
                        else {
                            return Config::parse_status::error(".ports[" + std::to_string(i) + "].port_min: Port out of range port_max");
                        }

                        port_res = field3.asUInt64();
                        if (0 < port_res && port_res < 65536) {
                            settings.port_max = (uint16_t)port_res;
                        }
                        else {
                            return Config::parse_status::error(".ports[" + std::to_string(i) + "].port_max: Port out of range port_max");
                        }

                        if (settings.port_max < settings.port_min) {
                            return Config::parse_status::error(".ports[" + std::to_string(i) + "]: Expected port_min <= port_max");
                        }
                    }
                    else {
                        return Config::parse_status::error(".ports[" + std::to_string(i) + "]: Expected integer field port_min and port_max");
                    }
                }
            }
        }
        else {
            return Config::parse_status::error(".ports: Expected array field");
        }

        return Config::parse_status::success();
    }

    void PermGroup::store(Json::Value& node) const {
        assert(node.empty());
        Permissions::store(node);
        node["name"] = name;

    }
    Config::parse_status PermGroup::parse(const Json::Value& node) {
        Config::parse_status res = Permissions::parse(node);
        if (!res) return res;

        Json::Value field;

        if (node.isMember("name") && (field = node["name"]).isString()) {
            name = field.asString();
        }
        else {
            name = "";
        }

        return Config::parse_status::success();
    }

    void User::store(Json::Value& node) const {
        assert(node.empty());
        PermGroup::store(node);
        Utils::Blob key_blob;
        if (key && key->export_public(key_blob)) {
            node["pukey"] = key_blob.to_string();
        }
        else {
            node["pukey"] = "PLACEHOLDER";
        }

    }
    Config::parse_status User::parse(const Json::Value& node) {
        Config::parse_status res = PermGroup::parse(node);
        if (!res) return res;

        Json::Value field;

        if (node.isMember("pukey") && (field = node["pukey"]).isString()) {
            Utils::Blob key_blob = Utils::Blob::factory_string(field.asString());
            std::shared_ptr<Crypto::PuKey> nkey = std::make_shared<Crypto::PuKey>();
            if (!nkey->load_public(key_blob)) {
                return Config::parse_status::error(".pukey: Could not parse key");
            }
            key = nkey;
        }
        else {
            return Config::parse_status::error(".pukey: Expected string field");
        }

        return Config::parse_status::success();
    }

    void UserGroup::store(Json::Value& node) const {
        assert(node.empty());
        node.clear();
        auto users_val = Json::Value(Json::arrayValue);
        for (auto&& it : _identites) {
            Json::Value it_val;
            it.second->store(it_val);
            users_val.append(it_val);
        }
        node["users"] = users_val;

    }
    Config::parse_status UserGroup::parse(const Json::Value& node) {
        Json::Value field;

        if (node.isMember("users")) {
            field = node["users"];
            if (field.isArray()) {
                assert(_identites.empty());
                _identites.clear();
                for (int i = 0; i < (int)field.size(); i++) {
                    Json::Value nodei = node[i];
                    std::shared_ptr<User> user = std::make_shared<User>();
                    Config::parse_status res = user->parse(nodei);
                    if (!res) return res.prefix(".users[" + std::to_string(i) + "]");
                    addUser(user);
                }
            }
            else if (field.isObject()) {
                std::shared_ptr<User> user = std::make_shared<User>();
                Config::parse_status res = user->parse(field);
                if (!res) return res.prefix(".users");
                addUser(user);
            }
            else {
                return Config::parse_status::error(".users: Expected array or object");
            }
        }
        else {
            return Config::parse_status::error(".users: Expected array or object");
        }

        return Config::parse_status::success();
    }

#ifdef WIN32
    std::string get_os_machine_name() {
        DWORD res_len = MAX_COMPUTERNAME_LENGTH + 1;
        Utils::string_os res;
        res.resize(res_len);

        // Get and display the name of the computer.
        if (0 != GetComputerNameW(res.data(), &res_len)) {
            res.resize(res_len);
            return Utils::convert_os2app(res);
        }

        return std::string();
    }
    std::string get_os_user_name() {
        DWORD res_len = UNLEN + 1;
        Utils::string_os res;
        res.resize(res_len);

        // Get and display the name of the computer.
        if (0 != GetUserNameW(res.data(), &res_len)) {
            res.resize(res_len);
            return Utils::convert_os2app(res);
        }

        return std::string();
    }
#elif __linux__
    std::string get_os_machine_name() {
        size_t res_len = HOST_NAME_MAX + 1;
        Utils::string_os res;
        res.resize(res_len);

        // Get and display the name of the computer.
        int code = gethostname(res.data(), res_len);
        if (0 == code) {
            res.resize(strlen(res.c_str()));
            return Utils::convert_os2app(res);
        }

        return std::string();
    }
    std::string get_os_user_name() {
        /*{
            //Gets the shelll user
            //On wsl, often doesnt work
            size_t res_len = LOGIN_NAME_MAX + 1;
            Utils::string_os res;
            res.resize(res_len);


            // Get and display the name of the computer.
            int code = getlogin_r(res.data(), res_len);
            if (0 == code) {
                res.resize(strlen(res.c_str()));
                return Utils::convert_os2app(res);
            }
        }*/

        {
            //Get name from the executing user

            passwd* res = getpwuid(getuid());

            std::string res_str(res->pw_name);

            //Do NOT need to free res.

            return res_str;
        }
    }
#endif
};