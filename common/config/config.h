#pragma once

#include "parse.h"

#include <filesystem>
#include <optional>
#include <variant>

#include "../identity/identity.h"

namespace NATBuster::Config {
   

    struct Host {
        std::string host_name = "";
        uint16_t port = 0;

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct ClientIPServerSettings {
        Host host;
        std::shared_ptr<Identity::UserGroup> identity = std::make_shared<Identity::UserGroup>();

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct ClientC2ServerSettings {
        Host host;
        std::shared_ptr<Identity::UserGroup> identity = std::make_shared<Identity::UserGroup>();
        bool auto_reconnect = false;
        int reconnect_interval = -1; //Seconds

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct BaseSettings {
    protected:
        BaseSettings() = default;
    public:
        std::string name = "";
        std::shared_ptr<Crypto::PrKey> prkey;
        //bool startup = false;

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct ClientSettings : public BaseSettings {
        ClientC2ServerSettings c2_server;
        std::vector<ClientIPServerSettings> ip_servers;

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct ServerSettings : public BaseSettings {
    protected:
        ServerSettings() = default;
    public:
        uint16_t port = 5987;

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct IPServerSettings : public ServerSettings {

    };

    struct C2ServerSettings : public ServerSettings {

    };

    struct ClientProfile {
        std::optional<ClientSettings> settings;
        std::filesystem::path path;
    };

    // %appdata%/NATBuster on windows
    // ~/.natbuster on linux
    std::filesystem::path get_config_folder();

    std::filesystem::path find_config_file(std::optional<std::filesystem::path> hint = std::nullopt);

    ClientProfile load_client_config(std::filesystem::path path);
    void save_client_config(const ClientProfile& profile);
}