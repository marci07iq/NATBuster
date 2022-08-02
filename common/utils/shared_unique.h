#pragma once

#include <memory>

namespace NATBuster::Common::Utils {
    template <typename T>
    class shared_unique_ptr : public std::shared_ptr<T> {
    public:
        shared_unique_ptr();

        shared_unique_ptr(T* t);

        shared_unique_ptr(std::unique_ptr<T>&& other) noexcept;
        template<typename CH>
        shared_unique_ptr(std::unique_ptr<CH>&& other) noexcept;

        shared_unique_ptr(shared_unique_ptr&& other) noexcept;
        template<typename CH>
        shared_unique_ptr(shared_unique_ptr<CH>&& other) noexcept;

        shared_unique_ptr& operator=(std::unique_ptr<T>&& other) noexcept;
        template<typename CH>
        shared_unique_ptr& operator=(std::unique_ptr<CH>&& other) noexcept;

        shared_unique_ptr& operator=(shared_unique_ptr&& other) noexcept;
        template<typename CH>
        shared_unique_ptr& operator=(shared_unique_ptr<CH>&& other) noexcept;

        std::shared_ptr<T> get_shared();
    };

    template <typename T>
    shared_unique_ptr<T>::shared_unique_ptr() : std::shared_ptr<T>() {

    }

    template <typename T>
    shared_unique_ptr<T>::shared_unique_ptr(T* t) : std::shared_ptr<T>(t) {

    }

    template <typename T>
    shared_unique_ptr<T>::shared_unique_ptr(std::unique_ptr<T>&& other) noexcept : std::shared_ptr<T>(other.release()) {

    }
    template <typename T>
    template <typename CH>
    shared_unique_ptr<T>::shared_unique_ptr(std::unique_ptr<CH>&& other) noexcept : std::shared_ptr<T>(other.release()) {

    }

    template <typename T>
    shared_unique_ptr<T>::shared_unique_ptr(shared_unique_ptr&& other) noexcept : std::shared_ptr<T>(other) {
        other.reset();
    }
    template <typename T>
    template <typename CH>
    shared_unique_ptr<T>::shared_unique_ptr(shared_unique_ptr<CH>&& other) noexcept : std::shared_ptr<T>(other) {
        other.reset();
    }

    template <typename T>
    shared_unique_ptr<T>& shared_unique_ptr<T>::operator=(std::unique_ptr<T>&& other) noexcept {
        this->reset(other.release());
        return *this;
    }
    template <typename T>
    template <typename CH>
    shared_unique_ptr<T>& shared_unique_ptr<T>::operator=(std::unique_ptr<CH>&& other) noexcept {
        this->reset(other.release());
        return *this;
    }

    template <typename T>
    shared_unique_ptr<T>& shared_unique_ptr<T>::operator=(shared_unique_ptr<T>&& other) noexcept {
        this->swap(other);
        other.reset();
        return *this;
    }
    template <typename T>
    template <typename CH>
    shared_unique_ptr<T>& shared_unique_ptr<T>::operator=(shared_unique_ptr<CH>&& other) noexcept {
        this->swap(other);
        other.reset();
        return *this;
    }

    template <typename T>
    std::shared_ptr<T> shared_unique_ptr<T>::get_shared() {
        return *this;
    }

}