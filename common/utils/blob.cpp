#include "blob.h"

namespace NATBuster::Common::Utils {
    
    //Consume a buffer, holding the data, but with pre and post gaps
    Blob::Blob(uint8_t* buffer_consume, uint32_t buffer_size, uint32_t data_size, uint32_t data_offset) : _buffer(buffer_consume), _offset(data_offset), _capacity(buffer_size), _size(data_size) {

    }

    //Consume a buffer, holding the data with no gaps
    Blob::Blob(uint8_t* buffer_consume, uint32_t buffer_size) : Blob(buffer_consume, buffer_size, buffer_size, 0) {

    }

    Blob::Blob() : Blob(nullptr, 0) {

    }
    Blob::Blob(Blob&& other) {
        _buffer = other._buffer;
        _offset = other._offset;
        _capacity = other._capacity;
        _size = other._size;

        other._buffer = nullptr;
        other._offset = 0;
        other._capacity = 0;
        other._size = 0;
    }
    Blob& Blob::operator=(Blob&& other) {
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

        return *this;
    }

    //Factory to consume an existing buffer
    Blob Blob::factory_consume(uint8_t* data_consume, uint32_t len) {
        return Blob(data_consume, len);
    }
    Blob Blob::factory_copy(uint8_t* buffer_copy, uint32_t buffer_size, uint32_t pre_gap, uint32_t end_gap) {
        uint32_t capacity = pre_gap + buffer_size + end_gap;
        uint8_t* buffer = new uint8_t[capacity];

        memcpy(buffer + pre_gap, buffer_copy, buffer_size);

        return Blob(buffer, capacity, buffer_size, pre_gap);
    }
    Blob Blob::factory_string(const std::string& str) {
        return factory_copy((uint8_t*)(str.c_str()), str.size(), 0, 0);
    }

    Blob Blob::concat(std::initializer_list<Blob*> list) {
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

    void Blob::grow(uint32_t min_pre_gap, uint32_t min_end_gap, bool smart) {
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
    
    //Start and end coordinates wrt old buffer indices
    void Blob::resize(int32_t new_start, uint32_t new_end) {
        dbg_self_test();

        uint32_t min_pre_gap = (new_start < 0) ? 0 : new_start;
        uint32_t min_end_gap = (new_end < _size) ? 0 : (new_end - _size);

        grow(min_pre_gap, min_end_gap, true);

        //Move start and end
        _offset = _offset + new_start;
        _size = new_end - new_start;

        dbg_self_test();
    }

    void Blob::add_blob_before(const Blob& other) {
        dbg_self_test();
        other.dbg_self_test();

        grow_pre_gap(other.size(), true);
        assert(other.size() <= _offset);

        memcpy(_buffer + (_offset - other.size()), other.get(), other.size());
        _offset = _offset - other.size();
        _size = other.size() + _size;

        dbg_self_test();
    }
    void Blob::add_blob_after(const Blob& other) {
        dbg_self_test();
        other.dbg_self_test();

        grow_end_gap(other.size(), true);
        assert(_offset + _size + other.size() <= _capacity);

        memcpy(_buffer + _offset + _size, other.get(), other.size());
        _size = _size + other.size();

        dbg_self_test();
    }
    void Blob::sandwich(const Blob& left, const Blob& right) {
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

    void Blob::clear() {
        if (_buffer != nullptr) {
            delete[] _buffer;
            _buffer = nullptr;
        }

        _offset = 0;
        _size = 0;
        _capacity = 0;
    }

    Blob::~Blob() {
        clear();
    }
}