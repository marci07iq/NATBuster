#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

namespace NATBuster::Common::Utils {

    class OnetimeWaker {
        std::atomic<bool> _val;

    public:
        OnetimeWaker() {
            _val.store(false);
        }

        void wake() {
            _val.store(true);
            _val.notify_all();
        }

        void wait() {
            _val.wait(false);
        }

        bool done() {
            return _val.load();
        }
    };

}