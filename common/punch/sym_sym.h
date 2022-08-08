#pragma once

#include "../network/network.h"
#include "../utils/waker.h"

#include <algorithm>
#include <random>


namespace NATBuster::Punch {
    //Class to perform the punch between two symmetric NATs
    //Opens hunders of outbound UDP ports, aimed at random ports on the target
    //By the birthday paradox, this should succeed after ~sqrt(65536) tries
    class HolepunchSym : public Utils::SharedOnly<HolepunchSym> {
        friend class Utils::SharedOnly<HolepunchSym>;
    public:
        using PunchCallback = Utils::Callback<Network::UDPHandleU>;

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
            Time::time_delta_type_us startup_delay = 0;

            //Total max time
            Time::time_delta_type_us timeout = 40000000; //40 sec: 30 for opening + 10 for waiting for remote
        };
    private:
        
        std::thread _thread;
        Utils::OnetimeWaker _waker;

        Utils::Blob _magic_ob;
        Utils::Blob _magic_ib;
        std::string _remote;
        HolepunchSymSettings _settings;

        PunchCallback _punch_callback;

        static void sync_launch_static(std::shared_ptr<HolepunchSym> obj);

        HolepunchSym(
            const std::string& remote,
            Utils::Blob&& magic_ob,
            Utils::Blob&& magic_ib,
            HolepunchSymSettings settings
        );

    public:
        static std::shared_ptr<HolepunchSym> create(
            const std::string& remote,
            Utils::Blob&& magic_ob,
            Utils::Blob&& magic_ib,
            HolepunchSymSettings settings = HolepunchSymSettings()
        );

        void sync_launch();

        void async_launch();

        bool done();

        void wait();

        inline void set_punch_callback(
            PunchCallback::raw_type punch_callback) {
            _punch_callback = punch_callback;
        }
    };
}