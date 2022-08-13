#include "config.h"

#include <fstream>

#include "../network/network.h"

#ifdef WIN32
#include <shlobj.h>
#endif

namespace NATBuster::Config {
    void Host::store(Json::Value& node) const {
        assert(node.empty());
        node.clear();
        node["host_name"] = host_name;
        node["port"] = port;
    }
    parse_status Host::parse(const Json::Value& node) {
        //Compact hostname
        if (node.isString()) {
            ErrorCode code = Network::NetworkAddress::split_network_string(node.asString(), host_name, port);
            if (code == ErrorCode::OK) return parse_status::success();
            return parse_status::error(": Could not parse address as string");
        }
        //Proper object
        else {
            Json::Value field;

            if (node.isMember("host_name") && (field = node["host_name"]).isString()) {
                host_name = field.asString();
            }
            else {
                return parse_status::error(".host_name: Expected string field");
            }

            if (node.isMember("port") && (field = node["port"]).isIntegral()) {
                uint64_t port_res = field.asUInt64();
                if (0 < port_res && port_res < 65536) {
                    port = (uint16_t)port_res;
                }
                else {
                    return parse_status::error(".port: Port out of range");
                }
            }
            else {
                return parse_status::error(".port: Expected integer field");
            }

            return parse_status::success();
        }
    }

    void ClientIPServerSettings::store(Json::Value& node) const {
        node.clear();
        {
            Json::Value field;
            host.store(field);
            node["host"] = field;
        }
        {
            Json::Value field;
            identity->store(field);
            node["identity"] = field;
        }
    }
    parse_status ClientIPServerSettings::parse(const Json::Value& node) {
        Json::Value field;

        if (node.isMember("host")) {
            field = node["host"];
            parse_status res = host.parse(field);
            if (!res) return res.prefix(".host");
        }

        if (node.isMember("identity")) {
            field = node["identity"];
            identity = std::make_shared<Identity::UserGroup>();
            parse_status res = identity->parse(field);
            if (!res) return res.prefix(".identity");
        }

        return parse_status::success();
    }

    void ClientC2ServerSettings::store(Json::Value& node) const {
        assert(node.empty());
        node.clear();
        {
            Json::Value field;
            host.store(field);
            node["host"] = field;
        }
        {
            Json::Value field;
            identity->store(field);
            node["identity"] = field;
        }
        node["auto_reconnect"] = auto_reconnect;
        node["reconnect_interval"] = reconnect_interval;
    }
    parse_status ClientC2ServerSettings::parse(const Json::Value& node) {
        Json::Value field;

        if (node.isMember("host")) {
            field = node["host"];
            parse_status res = host.parse(field);
            if (!res) return res.prefix(".host");
        }

        if (node.isMember("identity")) {
            field = node["identity"];
            identity = std::make_shared<Identity::UserGroup>();
            parse_status res = identity->parse(field);
            if (!res) return res.prefix(".identity");
        }

        if (node.isMember("auto_reconnect") && (field = node["auto_reconnect"]).isBool()) {
            auto_reconnect = field.asBool();
        }
        else {
            return parse_status::error(".auto_reconnect: Expected bool field");
        }

        if (auto_reconnect) {
            if (node.isMember("reconnect_interval") && (field = node["reconnect_interval"]).isIntegral()) {
                reconnect_interval = field.asInt();
                if (reconnect_interval < 5) {
                    return parse_status::error(".reconnect_interval: Expected >=5 seconds");
                }
            }
            else {
                return parse_status::error(".reconnect_interval: Expected int field");
            }
        }
        else {
            reconnect_interval = -1;
        }

        return parse_status::success();
    }

    void BaseSettings::store(Json::Value& node) const {
        assert(node.empty());
        node.clear();
        {
            node["name"] = name;
        }
        {
            Utils::Blob key_blob;
            if (prkey && prkey->export_private(key_blob)) {
                node["prkey"] = key_blob.to_string();
            }
            else {
                node["prkey"] = "PLACEHOLDER";
            }
        }
    }
    parse_status BaseSettings::parse(const Json::Value& node) {
        Json::Value field;

        if (node.isMember("name") && (field = node["name"]).isString()) {
            name = field.asString();
        }
        else {
            return parse_status::error(".name: Expected string field");
        }

        if (node.isMember("prkey") && (field = node["prkey"]).isString()) {
            Utils::Blob key_blob = Utils::Blob::factory_string(field.asString());
            std::shared_ptr<Crypto::PrKey> nkey = std::make_shared<Crypto::PrKey>();
            if (!nkey->load_private(key_blob)) {
                return parse_status::error(".prkey: Could not parse key");
            }
            prkey = nkey;
        }
        else {
            return parse_status::error(".name: Expected string field");
        }

        return parse_status::success();
    }

    void ClientSettings::store(Json::Value& node) const {
        BaseSettings::store(node);
        {
            Json::Value field;
            c2_server.store(field);
            node["c2_server"] = field;
        }
        {
            auto ip_servers_val = Json::Value(Json::arrayValue);
            for (auto&& it : ip_servers) {
                Json::Value it_val;
                it.store(it_val);
                ip_servers_val.append(it_val);
            }
            node["ip_servers"] = ip_servers_val;
        }
    }
    parse_status ClientSettings::parse(const Json::Value& node) {
        {
            parse_status res = BaseSettings::parse(node);
            if (!res) return res;
        }

        Json::Value field;

        if (node.isMember("c2_server")) {
            field = node["c2_server"];
            parse_status res = c2_server.parse(field);
            if (!res) return res.prefix(".c2_server");
        }

        if (node.isMember("ip_servers")) {
            field = node["ip_servers"];
            if (field.isArray()) {
                assert(ip_servers.empty());
                ip_servers.clear();
                for (int i = 0; i < (int)field.size(); i++) {
                    Json::Value nodei = node[i];
                    ClientIPServerSettings ip_server;
                    parse_status res = ip_server.parse(nodei);
                    if (!res) return res.prefix(".ip_servers[" + std::to_string(i) + "]");
                    ip_servers.push_back(ip_server);
                }
            }
            else if (field.isObject()) {
                ClientIPServerSettings ip_server;
                parse_status res = ip_server.parse(field);
                if (!res) return res.prefix(".ip_servers");
                ip_servers.push_back(ip_server);
            }
            else {
                return parse_status::error(".ip_servers: Expected array or object");
            }
        }
        else {
            return parse_status::error(".ip_servers: Expected array or object");
        }

        return parse_status::success();
    }

    void ServerSettings::store(Json::Value& node) const {
        BaseSettings::store(node);
        node["port"] = port;
    }
    parse_status ServerSettings::parse(const Json::Value& node) {
        parse_status res = BaseSettings::parse(node);
        if (!res) return res;

        Json::Value field;

        if (node.isMember("port") && (field = node["port"]).isIntegral()) {
            uint64_t port_res = field.asUInt64();
            if (0 < port_res && port_res < 65536) {
                port = (uint16_t)port_res;
            }
            else {
                return parse_status::error(".port: Port out of range");
            }
        }
        else {
            return parse_status::error(".port: Expected integer field");
        }

        return parse_status::success();
    }


#ifdef WIN32
    std::filesystem::path get_config_folder() {
        std::filesystem::path path;
        PWSTR path_tmp;

        auto get_folder_path_ret = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path_tmp);

        /* Error check */
        if (get_folder_path_ret != S_OK) {
            CoTaskMemFree(path_tmp);
            //Fatal error
            throw 1;
        }

        path = path_tmp;

        CoTaskMemFree(path_tmp);

        path = path / "NATBuster/";

        return path;
    }
#elif __linux__
    std::filesystem::path get_config_folder() {
        return "~/.natbuster/";
    }
#else
#error "Unsupported OS"
#endif

    std::filesystem::path find_config_file(std::optional<std::filesystem::path> hint) {
        if (hint) {
            std::filesystem::path path = std::filesystem::current_path() / hint.value();
            if (std::filesystem::is_regular_file(path)) return path;
        }

        return get_config_folder() / "client.json";
    }

    ClientProfile load_client_config(std::filesystem::path path) {
        ClientProfile profile;
        profile.path = path;
        profile.settings = std::nullopt;

        if (std::filesystem::is_regular_file(profile.path)) {
            Json::Value root;
            std::ifstream ifs(profile.path);

            Json::CharReaderBuilder builder;
            builder["collectComments"] = true;
            JSONCPP_STRING errs;
            if (!parseFromStream(builder, ifs, &root, &errs)) {
                std::cout << errs << std::endl;
                return profile;
            }

            profile.settings = ClientSettings();
            parse_status stat = profile.settings.value().parse(root);
            if (!stat) {
                std::cout << stat.message << std::endl;
                profile.settings = std::nullopt;
            }

        }
        return profile;
    }
    void save_client_config(const ClientProfile& profile) {
        if (profile.settings) {
            Json::Value root;
            Json::StreamWriterBuilder builder;
            const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

            std::filesystem::create_directories(profile.path.parent_path());
            std::ofstream ofs(profile.path);

            profile.settings.value().store(root);
            writer->write(root, &ofs);
        }

    }
};