#pragma once

#include "../common/network/network.h"
#include "../common/utils/waker.h"

#include <algorithm>
#include <random>


namespace NATBuster::Client {
    //Class to perform the punch between two symmetric NATs
    //Opens hunders of outbound UDP ports, aimed at random ports on the target
    //By the birthday paradox, this should succeed after ~sqrt(65536) tries
    class HolepunchSym : public std::enable_shared_from_this<HolepunchSym> {
    public:
        using PunchCallback = Common::Utils::Callback<Common::Network::UDPHandle>;

        struct HolepunchSymSettings {
            //The ephemeral range of the other router, to try and attack
            uint16_t port_min = 1024;
            uint16_t port_max = 65535;
            //For random search use 0
            uint16_t port_start = 0;
            float rate = 25.0f;

            //Approx exp(-750*750/65536) = 0.02% chance of failure
            uint16_t num_ports = 750;

            //Delay before punching begins
            Common::Time::time_delta_type_us startup_delay = 0;

            //Total max time
            Common::Time::time_delta_type_us timeout = 40000000; //40 sec: 30 for opening + 10 for waiting for remote
        };
    private:
        Common::Network::UDPHandle _socket;
        std::mutex _data_lock;

        std::thread _thread;
        Common::Utils::OnetimeWaker _waker;

        Common::Utils::Blob _magic_ob;
        Common::Utils::Blob _magic_ib;
        std::string _remote;
        HolepunchSymSettings _settings;

        PunchCallback _punch_callback;

        static void sync_launch_static(std::shared_ptr<HolepunchSym> obj);

        HolepunchSym(
            const std::string& remote,
            Common::Utils::Blob&& magic_ob,
            Common::Utils::Blob&& magic_ib,
            HolepunchSymSettings settings
        );

    public:
        static std::shared_ptr<HolepunchSym> create(
            const std::string& remote,
            Common::Utils::Blob&& magic_ob,
            Common::Utils::Blob&& magic_ib,
            HolepunchSymSettings settings = HolepunchSymSettings()
        );

        void sync_launch();

        void async_launch();

        bool done();

        void wait();

        Common::Network::UDPHandle get_socket();

        inline void set_punch_callback(
            PunchCallback::raw_type punch_callback) {
            _punch_callback = punch_callback;
        }
    };
}