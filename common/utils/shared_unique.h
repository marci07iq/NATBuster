#pragma once

#include <memory>

namespace NATBuster::Common::Utils {
    template <typename T>
    class shared_unique_ptr : public std::shared_ptr<T> {
    public:
        shared_unique_ptr() : std::shared_ptr<T>() {

        }
        shared_unique_ptr(T* t) : std::shared_ptr<T>(t) {

        }
        shared_unique_ptr(std::unique_ptr<T>&& other) : std::shared_ptr<T>(other.release()) {

        }
        shared_unique_ptr(shared_unique_ptr&& other) : std::shared_ptr<T>(other) {

        }
        shared_unique_ptr& operator=(std::unique_ptr<T>&& other) {
            std::shared_ptr<T>::reset(other.release());
        }
        shared_unique_ptr& operator=(shared_unique_ptr&& other) {
            std::shared_ptr<T>::reset(other.get());
            other.reset();
        }
    };
}