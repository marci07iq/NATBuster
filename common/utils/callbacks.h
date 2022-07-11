#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <numeric>
#include <optional>
#include <type_traits>
#include <variant>
#include <utility>

#include "copy_protection.h"

namespace NATBuster::Common::Utils {
    //Type erasure
    template <typename... ARGS>
    class CallbackBase {
    protected:
        CallbackBase() {

        }
    public:
        virtual void operator()(ARGS... args) const = 0;
    };

    //Empty callback, calls nothing
    template <typename... ARGS>
    class NoCallback : public CallbackBase<ARGS...> {
    public:

        NoCallback() {

        }
        inline void operator()(ARGS... args) const override {
            
        }
    };

    //Simple function pointer
    template <typename RET, typename... ARGS>
    class SimpleCallback : public CallbackBase<ARGS...> {
    public:
        using fn_type = RET(*)(ARGS...);
    private:
        fn_type _fn;
    public:

        SimpleCallback(fn_type fn) : _fn(fn) {

        }
        inline void operator()(ARGS... args) const override {
            _fn(std::forward<ARGS>(args)...);
        }
    };

    //std::function callback
    template <typename RET, typename... ARGS>
    class FunctionalCallback : public CallbackBase<ARGS...> {
    public:
        using fn_type = std::function<RET(ARGS...)>;
    private:
        fn_type _fn;
    public:

        FunctionalCallback(fn_type fn) : _fn(fn) {

        }
        inline void operator()(ARGS... args) const override {
            _fn(std::forward<ARGS>(args)...);
        }
    };

    //Member function callback, via a weak pointer
    template <typename CLASS, typename RET, typename... ARGS>
    class MemberCallback : public CallbackBase<ARGS...> {
    public:
        using fn_type = RET(CLASS::*)(ARGS...);
    private:
        fn_type _fn;
        std::weak_ptr<CLASS> _inst;
    public:

        MemberCallback(std::weak_ptr<CLASS> inst, fn_type fn) : _inst(inst), _fn(fn) {

        }
        inline void operator()(ARGS... args) const override {
            std::shared_ptr<CLASS> inst = _inst.lock();
            if (inst) {
                (inst.get()->*(_fn))(std::forward<ARGS>(args)...);
            }
        }
    };

    //Safe to call even when callback is null (just wont do anything)
    //Can safely be replaced via operator= from any thread at any time (while object is alive)
    //Even from the callback itself
    template <typename... ARGS>
    class Callback : Utils::NonCopyable {
    public:
        using raw_type = CallbackBase<ARGS...>*;
    private:
        //Callback to run, if no _new_cb set
        raw_type _cb = nullptr;
        //To replace _cb with before the next run
        std::atomic<raw_type> _new_cb = nullptr;
        
        //If you cant compile on your platform, you can remove this line
        //It is only to make sure things are fast
        static_assert(decltype(_new_cb)::is_always_lock_free);

    public:
        void replace(raw_type cb) {
            //Set new callback
            raw_type old_new_cb = _new_cb.exchange(cb);
            //Remove if there was a previos new callback (replaced multiple times between calls)
            if (old_new_cb != nullptr) {
                delete old_new_cb;
            }

        }

        //Set to null

        Callback() {
            _cb = nullptr;
        }
        Callback(const std::nullptr_t& null) {
            _cb = nullptr;
        }
        Callback& operator=(const std::nullptr_t& null) {
            replace(nullptr);
            return *this;
        }

        //Set to user set fn

        //Pass in a new pointer, and do NOT keep any other refernce to it.
        Callback(raw_type cb) {
            _cb = cb;
        }
        //Pass in a new pointer, and do NOT keep any other refernce to it.
        Callback& operator=(raw_type cb) {
            replace(cb);
            return *this;
        }

        //For one time use timers
        void call_and_clear(ARGS... args) {
            if (this != nullptr) {
                //Check out new cb
                raw_type new_cb = _new_cb.exchange(nullptr);
                //New callback has been set
                if (new_cb != nullptr) {
                    //Remove old one
                    if (_cb != nullptr) {
                        delete _cb;
                    }
                    //Set new one
                    _cb = new_cb;
                }

                //Check out cb
                raw_type cb = _cb;
                _cb = nullptr;
                if (cb != nullptr) {
                    cb->operator()(std::forward<ARGS>(args)...);
                    delete cb;
                }
            }
        }

        void operator()(ARGS... args) {
            if (this != nullptr) {
                //Check out new cb
                raw_type new_cb = _new_cb.exchange(nullptr);
                //New callback has been set
                if (new_cb != nullptr) {
                    //Remove old one
                    if (_cb != nullptr) {
                        delete _cb;
                    }
                    //Set new one
                    _cb = new_cb;
                }

                //Call
                if (_cb != nullptr) {
                    _cb->operator()(std::forward<ARGS>(args)...);
                }
            }
        }

        bool has_function() {
            return (_cb != nullptr) || (_new_cb.load() != nullptr);
        }

        ~Callback() {
            if (_cb != nullptr) {
                delete _cb;
                _cb = nullptr;
            }

            raw_type new_cb = _new_cb.load();
            if (new_cb != nullptr) {
                delete new_cb;
                new_cb = nullptr;
            }
        }
    };
}