#pragma once

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


    class AbstractBase {
    protected:
        AbstractBase() = default;
        AbstractBase(const AbstractBase&) = default;
        AbstractBase(AbstractBase&&) = default;
    };
};