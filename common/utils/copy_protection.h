#pragma once

#include "shared_unique.h"

namespace NATBuster::Common::Utils {
    //Class that cant be copied
    class NonCopyable
    {
    public:
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator = (const NonCopyable&) = delete;

    protected:
        NonCopyable() = default;
        ~NonCopyable() = default; /// Protected non-virtual destructor
    };

    //Class that can only be created via pointer case (T*)(other)
    //Intended for mapping structs as "views" onto binary data blob
    class NonStack
    {
    private:
        NonStack() = delete;
        NonStack(const NonStack&) = delete;
        NonStack(NonStack&&) = delete;

        NonStack& operator= (const NonStack&) = delete;
        NonStack& operator= (NonStack&&) = delete;

        ~NonStack() = delete;
    };

    //Object that can only be consturcted as a shared_ptr
    //Calls init before returning from create
    template <typename SELF>
    class SharedOnly : public std::enable_shared_from_this<SELF> {
    private:

    protected:
        SharedOnly() {

        }
    public:
        template<typename ...ARGS>
        shared_unique_ptr<SELF> create(ARGS... args) {
            shared_unique_ptr<SELF> res = shared_unique_ptr<SELF>(new SELF(std::forward(args...)));
            res->init();
            return res;
        }
    };


};