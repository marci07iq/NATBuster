#pragma once

#include <memory>

#include "../utils/event.h"
#include "../utils/time.h"
#include "../utils/callbacks.h"

namespace NATBuster::Common::Transport {
    /*template <typename T>
    concept Holdable = requires(
        T & t,
        Utils::CallbackBase<>*cb_ka,
        Utils::Timers::TimerCallback::raw_type cb_tm,
        Time::time_delta_type_us delta,
        Time::time_type_us end)
    {
        //Callback asking to keep socket alive
        { t.set_keepalive_callback(cb_ka) } -> std::same_as<void>;
        //Ability to set callbacks
        { t.addDelay(cb_tm, delta) } -> std::same_as<void>;
        { t.addTimer(cb_tm, end) } -> std::same_as<void>;
    };

    //Class to hold an object for a given lifetime
    template <typename T>
    class Holder {
        std::shared_ptr<T> _content;

        std::shared_ptr<Holder<T>> _self;
    public:

    };*/
};