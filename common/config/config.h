#pragma once

#include "parse.h"

#include "../identity/identity.h"

namespace NATBuster::Config {
   

    struct Host {
        std::string host_name;
        uint16_t port;

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct ClientIPServerSettings {
        Host host;
        std::shared_ptr<Identity::UserGroup> identity;

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct ClientC2ServerSettings {
        Host host;
        std::shared_ptr<Identity::UserGroup> identity;
        bool auto_reconnect;
        int reconnect_interval; //Seconds

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };

    struct ClientSettings {
        ClientC2ServerSettings c2_server;
        std::vector<ClientIPServerSettings> ip_servers;
        std::string name;
        std::shared_ptr<Crypto::PrKey> prkey;

        void store(Json::Value& node) const;
        parse_status parse(const Json::Value& node);
    };
}