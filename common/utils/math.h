#pragma once

#include <limits>

namespace NATBuster::Utils {
    //Add two numbers, without going outside the limits
    //Safe even when operation would normally overflow
    template<typename T>
    T clamped_add(T lhs, T rhs) {
        //This class assumes that 0 is within the representable range of the type
        static_assert(std::numeric_limits<T>::lowest() <= 0); //For floats, lowest is the appropriate way to get min
        static_assert(0 <= std::numeric_limits<T>::max());

        if (lhs > 0) {
            //We need to be worried about an overflow
            if (rhs > 0) {
                //Maximum value that can be added to lhs without an over/underflow
                //Since 0 < lhs < ::max
                //0 < ::max - lhs < ::max
                T room_left = std::numeric_limits<T>::max() - lhs;
                //Safe to add
                if (rhs <= room_left) {
                    return lhs + rhs;
                }
                else {
                    return std::numeric_limits<T>::max();
                }
            }
            //If ::min <= rhs <= 0 < lhs <= ::max
            //Then ::min < lhs + rhs <= ::max, so no need to worry about an overflow
        }
        else if (lhs < 0) {
            //We need to be worried about an underflow
            if (rhs < 0) {
                //Largest negative value that can be added
                //Since ::min < lhs < 0
                //::min < ::min - lhs < 0
                T room_left = std::numeric_limits<T>::lowest() - lhs;
                //Safe to add
                if (rhs >= room_left) {
                    return lhs + rhs;
                }
                else {
                    return std::numeric_limits<T>::lowest();
                }
            }
            //If ::min <= lhs < 0 <= rhs <= ::max
            //Then ::min <= lhs + rhs < ::max, so no need to worry about an over/underflow
        }

        //Can never over/underflow
        return lhs + rhs;
    }

    //Subtract two numbers, without going outside the limits
    //Safe even when operation would normally overflow
    template<typename T>
    T clamped_sub(T lhs, T rhs) {
        //This class assumes that 0 is within the representable range of the type
        static_assert(std::numeric_limits<T>::lowest() <= 0); //For floats, lowest is the appropriate way to get min
        static_assert(0 <= std::numeric_limits<T>::max());
        //Hence ::min() + ::max() is also within the range

        if (rhs > 0) {
            //We need to be worried about an underflow
            if (lhs < (std::numeric_limits<T>::lowest() + std::numeric_limits<T>::max())) {
                //Since ::min <= lhs < (::min + ::max)
                //0 < (::min + ::max) - lhs <= ::max
                T a = (std::numeric_limits<T>::lowest() + std::numeric_limits<T>::max()) - lhs;
                //Since 0 < rhs <= ::max
                //0 <= ::max - rhs < ::max
                T b = std::numeric_limits<T>::max() - rhs;
                //a <= b
                //(::min() + ::max()) - lhs <= ::max() - rhs
                //(::min()) - lhs <= - rhs
                //(::min()) <= lhs - rhs
                //Safe to subtract
                if (a <= b) {
                    return lhs - rhs;
                }
                else {
                    return std::numeric_limits<T>::lowest();
                }
            }
        }
        else if (rhs < 0) {
            //We need to be worried about an overflow
            if ((std::numeric_limits<T>::lowest() + std::numeric_limits<T>::max()) < lhs) {
                //Since (::min + ::max) < lhs <= ::max
                //::min <= (::min + ::max) - lhs < 0
                T a = (std::numeric_limits<T>::lowest() + std::numeric_limits<T>::max()) - lhs;
                //Since ::min <= rhs < 0
                //::min < ::min - rhs <= 0
                T b = std::numeric_limits<T>::lowest() - rhs;
                //a >= b
                //(::min + ::max) - lhs >= ::min - rhs
                //::max - lhs >= - rhs
                //::max >= lhs - rhs
                //Safe to subtract
                if (a >= b) {
                    return lhs - rhs;
                }
                else {
                    return std::numeric_limits<T>::max();
                }
            }
        }

        //Can never over/underflow
        return lhs + rhs;
    }



    //Add two numbers, without going outside the user defined limits
    //Safe even when operation would normally overflow
    template<typename T>
    T clamped_add(T lhs, T rhs, T min, T max) {
        T res = clamped_add(lhs, rhs);
        //Clamp to user defined range
        return (res < max) ? ((res < min) ? min : res) : max;
    }

    //Subtract two numbers, without going outside the user defined limits
    //Safe even when operation would normally overflow
    template<typename T>
    T clamped_sub(T lhs, T rhs, T min, T max) {
        T res = clamped_sub(lhs, rhs);
        //Clamp to user defined range
        return (res < max) ? ((res < min) ? min : res) : max;
    }
}