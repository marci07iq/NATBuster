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

namespace NATBuster::Utils {
    //Type erasure
    template <typename... ARGS>
    class CallbackBase {
    protected:
        CallbackBase() {

        }
    public:
        virtual void operator()(ARGS... args) const = 0;

        virtual ~CallbackBase() = default;
    };

    //Empty callback, calls nothing
    template <typename... ARGS>
    class NoCallback : public CallbackBase<ARGS...> {
    public:

        NoCallback() {

        }
        inline void operator()([[maybe_unused]]  ARGS... args) const override {
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

    //Member function callback, via a raw pointer
    template <typename CLASS, typename RET, typename... ARGS>
    class MemberRCallback : public CallbackBase<ARGS...> {
    public:
        using fn_type = RET(CLASS::*)(ARGS...);
    private:
        fn_type _fn;
        CLASS* _inst;
    public:

        MemberRCallback(CLASS* inst, fn_type fn) : _inst(inst), _fn(fn) {

        }
        inline void operator()(ARGS... args) const override {
            if (_inst != nullptr) {
                (_inst->*(_fn))(std::forward<ARGS>(args)...);
            }
        }
    };

    //Member function callback, via a weak pointer
    template <typename CLASS, typename RET, typename... ARGS>
    class MemberWCallback : public CallbackBase<ARGS...> {
    public:
        using fn_type = RET(CLASS::*)(ARGS...);
    private:
        fn_type _fn;
        std::weak_ptr<CLASS> _inst;
    public:

        MemberWCallback(std::weak_ptr<CLASS> inst, fn_type fn) : _inst(inst), _fn(fn) {

        }
        inline void operator()(ARGS... args) const override {
            std::shared_ptr<CLASS> inst = _inst.lock();
            if (inst) {
                (inst.get()->*(_fn))(std::forward<ARGS>(args)...);
            }
        }
    };

    //Member function callback, via a strong pointer
    template <typename CLASS, typename RET, typename... ARGS>
    class MemberSCallback : public CallbackBase<ARGS...> {
    public:
        using fn_type = RET(CLASS::*)(ARGS...);
    private:
        fn_type _fn;
        std::shared_ptr<CLASS> _inst;
    public:

        MemberSCallback(std::shared_ptr<CLASS> inst, fn_type fn) : _inst(inst), _fn(fn) {

        }
        inline void operator()(ARGS... args) const override {
            if (_inst) {
                (_inst.get()->*(_fn))(std::forward<ARGS>(args)...);
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
        static NoCallback<ARGS...> checkout_cb;
        //An other constant like nullptr meaning that the callback is currently running
        //Do not delete this value.
        static const raw_type checkout_ptr;

        //The currently set callback
        std::atomic<raw_type> _cb = nullptr;

        //If you cant compile on your platform, you can remove this line
        //It is only to make sure things are fast
        static_assert(decltype(_cb)::is_always_lock_free);

        inline static bool valid(raw_type cb) {
            return (cb != nullptr && cb != checkout_ptr);
        }

        //Helper to dispose of unneeded raw ptrs
        //Safe to call with null or checkout
        inline static void dispose(raw_type cb) {
            if (valid(cb)) {
                delete cb;
            }
        }

        inline void replace(raw_type cb) {
            //Set new callback, and dispose of old one
            dispose(_cb.exchange(cb));
        }

        inline raw_type extract() {
            //Extract the new callback, and leave nullptr
            raw_type old_cb = _cb.exchange(nullptr);
            //If checked out, can't extract, and must return null
            if (old_cb == checkout_ptr) return nullptr;
            return old_cb;
        }

        inline raw_type checkout() {
            //Extract the new callback, and leave nullptr
            raw_type old_cb = _cb.exchange(checkout_ptr);
            //If checked out, can't extract, and must return null
            if (old_cb == checkout_ptr) return nullptr;
            return old_cb;
        }

        inline void checkback(raw_type back) {
            raw_type found = checkout_ptr;
            //Try to swap back
            if (!_cb.compare_exchange_strong(found, back)) {
                //If couldnt swap back, dispose of old value
                dispose(back);
            }
        }
    public:
        //Set to null

        Callback() {
            _cb = nullptr;
        }
        Callback(const std::nullptr_t& null) {
            _cb = nullptr;
        }

        //Set to user set fn

        //Pass in a new pointer, and do NOT keep any other refernce to it.
        Callback(raw_type cb) {
            _cb = cb;
        }
        //Completely thread safe
        //Pass in a new pointer, and do NOT keep any other refernce to it.
        //Note: passing in nullptr is undefined behaviour
        Callback& operator=(raw_type cb) {
            assert(cb != nullptr);
            replace(cb);
            return *this;
        }

        //Completely threadsafe for *this
        //Safe to call on any data.
        //WARNING: If data is currently executing a callback, both callbacks will become null.
        void move_from_safe_other(Callback& data) {
            replace(data.extract());
        }

        //Complety threadsafe
        void clear() {
            replace(nullptr);
        }

        //Complety threadsafe
        void call_and_clear(ARGS... args) {
            raw_type cb = extract();
            if (cb != nullptr && cb != checkout_ptr) {
                cb->operator()(std::forward<ARGS>(args)...);
            }
            dispose(cb);
        }

        //Complety threadsafe
        void call(ARGS... args) {
            raw_type cb = checkout();
            if (cb != nullptr && cb != checkout_ptr) {
                cb->operator()(std::forward<ARGS>(args)...);
            }
            checkback(cb);
        }

        bool has() {
            return valid(_cb);
        }

        //Complety threadsafe
        inline void operator()(ARGS... args) {
            call(std::forward<ARGS>(args)...);
        }

        ~Callback() {
            replace(nullptr);
        }
    };

    template <typename... ARGS>
    NoCallback<ARGS...> Callback<ARGS...>::checkout_cb = NoCallback<ARGS...>();
    template <typename... ARGS>
    const typename Callback<ARGS...>::raw_type Callback<ARGS...>::checkout_ptr = &Callback<ARGS...>::checkout_cb;
}