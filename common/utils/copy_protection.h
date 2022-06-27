#pragma once

namespace NATBuster::Common::Utils {
    class NonCopyable
    {
    public:
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator = (const NonCopyable&) = delete;

    protected:
        NonCopyable() = default;
        ~NonCopyable() = default; /// Protected non-virtual destructor
    };

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
};