
#include <assert.h>
#include <cstdint>
#include <memory>

#include "copy_protection.h"

namespace NATBuster::Common::Utils {
    class Blob : Utils::MoveableNonCopyable
    {
        uint8_t* _buffer;
        uint32_t _offset;
        uint32_t _capacity;
        uint32_t _size;

        inline void dbg_self_test() const {
#ifndef NDEBUG
            assert(_buffer != nullptr);
            assert(_offset <= _capacity);
            assert(_size <= _capacity);
            assert(_offset + _size <= _capacity);
#endif
        }

        Blob(uint8_t* buffer_consume, uint32_t buffer_size, uint32_t data_size, uint32_t data_offset = 0) : _buffer(buffer_consume), _offset(data_offset), _capacity(buffer_size), _size(data_size) {

        }

        Blob(uint8_t* buffer_consume, uint32_t buffer_size) : Blob(buffer_consume, buffer_size, buffer_size, 0) {

        }

        inline uint32_t get_end_gap() const {
            return _capacity - _offset - _size;
        }

    public:
        Blob() : Blob(nullptr, 0) {

        }

        Blob(Blob&& other) {
            _buffer = other._buffer;
            _offset = other._offset;
            _capacity = other._capacity;
            _size = other._size;

            other._buffer = nullptr;
            other._offset = 0;
            other._capacity = 0;
            other._size = 0;
        }

        Blob& operator=(Blob&& other) {
            //Wipe self content
            clear();

            _buffer = other._buffer;
            _offset = other._offset;
            _capacity = other._capacity;
            _size = other._size;

            other._buffer = nullptr;
            other._offset = 0;
            other._capacity = 0;
            other._size = 0;
        }

        //Factory to consume an existing buffer
        static Blob consume(uint8_t* data_consume, uint32_t len) {
            return Blob(data_consume, len);
        }

        static Blob concat(std::initializer_list<Blob*> list) {
            uint32_t new_offset = 0;
            uint32_t new_len = 0;
            for (auto& it : list) {
                new_len += it->size();
            }
            //New end gap
            uint32_t new_cap = new_offset + new_len + 0;

            uint8_t* buffer = new uint8_t[new_len];

            uint32_t progress = new_offset;

            for (auto it : list) {
                memcpy(buffer + progress, it->get(), it->size());
                new_len += it->size();
            }

            return Blob(buffer, new_cap, new_len, new_offset);
        }

        void grow(uint32_t min_pre_gap, uint32_t min_end_gap, bool smart = true) {
            dbg_self_test();

            uint32_t new_offset = (_offset < min_pre_gap) ?
                (
                    smart ?
                    (min_pre_gap + (min_pre_gap - _offset)) :
                    min_pre_gap
                    ) :
                _offset;

            uint32_t old_end_gap = get_end_gap();

            uint32_t new_end_gap = (old_end_gap < min_end_gap) ?
                (
                    smart ?
                    (min_end_gap + (min_end_gap - old_end_gap)) :
                    min_end_gap
                    ) :
                old_end_gap;

            uint32_t new_capacity = new_offset + _size + new_end_gap;
            if (new_capacity > _capacity) {
                
                uint8_t* new_buffer = new uint8_t[new_capacity];

                //Don't bother copying the part of the buffer that is not in the actively used region.
                memcpy(new_buffer + new_offset, _buffer + _offset, _size);

                delete[] _buffer;
                _buffer = nullptr;

                _buffer = new_buffer;
                _capacity = new_capacity;
                _offset = new_offset;
                assert(new_end_gap == get_end_gap());
            }

            dbg_self_test();

            assert(min_pre_gap <= _offset);
            assert(min_end_gap <= get_end_gap());
        }

        inline void grow_end_gap(uint32_t min_end_gap, bool smart = true) {
            grow(0, min_end_gap, smart);
        }

        inline void grow_pre_gap(uint32_t min_pre_gap, bool smart = true) {
            grow(min_pre_gap, 0, smart);
        }

        void add_blob_after(const Blob& other) {
            dbg_self_test();
            other.dbg_self_test();

            grow_end_gap(other.size(), true);
            assert(_offset + _size + other.size() <= _capacity);

            memcpy(_buffer + _offset + _size, other.get(), other.size());
            _size = _size + other.size();

            dbg_self_test();
        }

        void add_blob_before(const Blob& other) {
            dbg_self_test();
            other.dbg_self_test();

            grow_pre_gap(other.size(), true);
            assert(other.size() <= _offset);

            memcpy(_buffer + (_offset - other.size()), other.get(), other.size());
            _offset = _offset - other.size();
            _size = other.size() + _size;

            dbg_self_test();
        }

        void sandwich(const Blob& left, const Blob& right) {
            dbg_self_test();
            left.dbg_self_test();
            right.dbg_self_test();

            grow(left.size(), right.size(), true);
            assert(left.size() <= _offset);
            assert(right.size() <= get_end_gap());

            memcpy(_buffer + (_offset - left.size()), left.get(), left.size());
            memcpy(_buffer + _offset + _size, right.get(), right.size());
            _offset = _offset - left.size();
            _size = left.size() + _size + right.size();

            dbg_self_test();
        }

        inline uint8_t* get() {
            dbg_self_test();
            if (_buffer == nullptr) return nullptr;
            return _buffer + _offset;
        }

        inline const uint8_t* get() const {
            dbg_self_test();
            if (_buffer == nullptr) return nullptr;
            return _buffer + _offset;
        }

        inline const uint32_t size() const {
            return _size;
        }

        //Start and end coordinates wrt old buffer indices
        void resize(int32_t new_start, uint32_t new_end) {
            dbg_self_test();

            uint32_t min_pre_gap = (new_start < 0) ? 0 : new_start;
            uint32_t min_end_gap = (new_end < _size) ? 0 : (new_end - _size);

            grow(min_pre_gap, min_end_gap, true);

            //Move start and end
            _offset = _offset + new_start;
            _size = new_end - new_start;

            dbg_self_test();
        }

        void clear() {
            if (_buffer != nullptr) {
                delete[] _buffer;
                _buffer = nullptr;
            }

            _offset = 0;
            _size = 0;
            _capacity = 0;
        }

        ~Blob() {
            clear();
        }
    };
}