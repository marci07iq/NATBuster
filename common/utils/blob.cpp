#include "blob.h"

namespace NATBuster::Utils {
    
    _ConstBlobView::_ConstBlobView() {

    }

    ConstBlobSliceView _ConstBlobView::cslice(uint32_t start, uint32_t len) const {
        return ConstBlobSliceView(this, start, len);
    }

    ConstBlobSliceView _ConstBlobView::cslice_left(uint32_t split) const {
        assert(split <= size());
        return ConstBlobSliceView(this, 0, split);
    }

    ConstBlobSliceView _ConstBlobView::cslice_right(uint32_t split) const {
        assert(split <= size());
        return ConstBlobSliceView(this, split, size() - split);
    }

    bool operator==(const ConstBlobView& lhs, const ConstBlobView& rhs) {
        if (lhs.size() != rhs.size()) return false;
        return 0 == memcmp(lhs.getr(), rhs.getr(), lhs.size());
    }

    bool operator!=(const ConstBlobView& lhs, const ConstBlobView& rhs) {
        if (lhs.size() == rhs.size()) return true;
        return 0 != memcmp(lhs.getr(), rhs.getr(), lhs.size());
    }

    std::strong_ordering operator<=>(const ConstBlobView& lhs, const ConstBlobView& rhs) {
        const uint8_t* buf_lhs = lhs.getr();
        const uint8_t* buf_rhs = rhs.getr();
        for (uint32_t i = 0; i < lhs.size() && i < rhs.size(); i++) {
            std::strong_ordering res = buf_lhs[i] <=> buf_rhs[i];
            if (res != std::strong_ordering::equal) {
                return res;
            }
        }
        return lhs.size() <=> rhs.size();
    }

    BlobView::BlobView() {

    }

    BlobSliceView BlobView::slice(uint32_t start, uint32_t len) {
        return BlobSliceView(this, start, len);
    }

    BlobSliceView BlobView::slice_left(uint32_t split) {
        assert(split <= size());
        return BlobSliceView(this, 0, split);
    }
    
    BlobSliceView BlobView::slice_right(uint32_t split) {
        assert(split <= size());
        return BlobSliceView(this, split, size() - split);
    }

    


    //Consume a buffer, holding the data, but with pre and post gaps
    Blob::Blob(uint8_t* buffer_consume, uint32_t buffer_size, uint32_t data_size, uint32_t data_pre_gap) :
        _buffer(buffer_consume),
        _pre_gap(data_pre_gap),
        _capacity(buffer_size),
        _size(data_size) {

    }

    //Consume a buffer, holding the data with no gaps
    Blob::Blob(uint8_t* buffer_consume, uint32_t buffer_size) :
        Blob(buffer_consume, buffer_size, buffer_size, 0) {

    }

    Blob::Blob() : Blob(nullptr, 0) {

    }
    Blob::Blob(Blob&& other) noexcept {
        _buffer = other._buffer;
        _pre_gap = other._pre_gap;
        _capacity = other._capacity;
        _size = other._size;

        other._buffer = nullptr;
        other._pre_gap = 0;
        other._capacity = 0;
        other._size = 0;
    }
    Blob& Blob::operator=(Blob&& other) noexcept {
        //Wipe self content
        clear();

        _buffer = other._buffer;
        _pre_gap = other._pre_gap;
        _capacity = other._capacity;
        _size = other._size;

        other._buffer = nullptr;
        other._pre_gap = 0;
        other._capacity = 0;
        other._size = 0;

        return *this;
    }

    Blob Blob::factory_empty(uint32_t size, uint32_t pre_gap, uint32_t end_gap) {
        uint32_t capacity = size + pre_gap + end_gap;
        uint8_t* data = Blob::alloc(capacity);
        return Blob(data, capacity, size, pre_gap);
    }
    Blob Blob::factory_consume(uint8_t* data_consume, uint32_t len) {
        return Blob(data_consume, len);
    }
    Blob Blob::factory_copy(const uint8_t* buffer_copy, uint32_t buffer_size, uint32_t pre_gap, uint32_t end_gap) {
        uint32_t capacity = pre_gap + buffer_size + end_gap;
        uint8_t* buffer = Blob::alloc(capacity);

        Blob::bufcpy(buffer, capacity, pre_gap, buffer_copy, buffer_size, 0, buffer_size);

        return Blob(buffer, capacity, buffer_size, pre_gap);
    }
    Blob Blob::factory_copy(const ConstBlobView& view, uint32_t pre_gap, uint32_t end_gap) {
        return Blob::factory_copy(view.getr(), view.size(), pre_gap, end_gap);
    }
    Blob Blob::factory_string(const std::string& str) {
        assert(str.size() < std::numeric_limits<uint32_t>::max());
        return factory_copy((uint8_t*)(str.c_str()), (uint32_t)str.size(), 0, 0);
    }

    Blob Blob::factory_concat(std::initializer_list<ConstBlobView*> list, uint32_t pre_gap, uint32_t end_gap) {
        uint32_t new_len = 0;
        for (auto it : list) {
            new_len += it->size();
        }
        //New capacity
        uint32_t new_cap = pre_gap + new_len + end_gap;

        uint8_t* buffer = Blob::alloc(new_cap);

        //Write progress
        uint32_t progress = pre_gap;

        for (auto it : list) {
            Blob::bufcpy(buffer, new_cap, progress, it->getr(), it->size(), 0, it->size());
            progress += it->size();
        }

        return Blob(buffer, new_cap, new_len, pre_gap);
    }

    void Blob::grow_gap(uint32_t min_pre_gap, uint32_t min_end_gap, bool smart) {
        dbg_self_test();

        uint32_t new_pre_gap = (_pre_gap < min_pre_gap) ?
            (
                smart ?
                (min_pre_gap + (min_pre_gap - _pre_gap)) :
                min_pre_gap
                ) :
            _pre_gap;

        uint32_t old_end_gap = get_end_gap();

        uint32_t new_end_gap = (old_end_gap < min_end_gap) ?
            (
                smart ?
                (min_end_gap + (min_end_gap - old_end_gap)) :
                min_end_gap
                ) :
            old_end_gap;

        uint32_t new_capacity = new_pre_gap + _size + new_end_gap;
        if (new_capacity > _capacity) {

            uint8_t* new_buffer = Blob::alloc(new_capacity);

            //Don't bother copying the part of the buffer that is not in the actively used region.
            Blob::bufcpy(new_buffer, new_capacity, new_pre_gap, _buffer, _capacity, _pre_gap, _size);

            delete[] _buffer;
            _buffer = nullptr;

            _buffer = new_buffer;
            _capacity = new_capacity;
            _pre_gap = new_pre_gap;
            assert(new_end_gap == get_end_gap());
        }

        dbg_self_test();

        assert(min_pre_gap <= _pre_gap);
        assert(min_end_gap <= get_end_gap());
    }
    
    //Start and end coordinates wrt old buffer indices
    void Blob::resize(int32_t new_start, uint32_t new_end) {
        dbg_self_test();

        uint32_t min_pre_gap = (new_start < 0) ? (-new_start) : 0;
        uint32_t min_end_gap = (new_end < _size) ? 0 : (new_end - _size);

        grow_gap(min_pre_gap, min_end_gap, true);

        //Move start and end
        _pre_gap = _pre_gap + new_start;
        _size = new_end - new_start;

        dbg_self_test();
    }

    void Blob::add_blob_before(const ConstBlobView& other) {
        dbg_self_test();

        grow_pre_gap(other.size(), true);
        assert(other.size() <= _pre_gap);

        Blob::bufcpy(
            _buffer, _capacity, _pre_gap - other.size(),
            other.getr(), other.size(), 0,
            other.size());
        _pre_gap = _pre_gap - other.size();
        _size = other.size() + _size;

        dbg_self_test();
    }
    void Blob::add_blob_after(const ConstBlobView& other) {
        dbg_self_test();

        grow_end_gap(other.size(), true);
        assert(_pre_gap + _size + other.size() <= _capacity);

        Blob::bufcpy(
            _buffer, _capacity, _pre_gap + _size,
            other.getr(), other.size(), 0,
            other.size());
        _size = _size + other.size();

        dbg_self_test();
    }
    void Blob::sandwich(const ConstBlobView& left, const BlobView& right) {
        dbg_self_test();
        
        grow_gap(left.size(), right.size(), true);
        assert(left.size() <= _pre_gap);
        assert(right.size() <= get_end_gap());

        Blob::bufcpy(
            _buffer, _capacity, _pre_gap - left.size(),
            left.getr(), left.size(), 0,
            left.size());
        Blob::bufcpy(
            _buffer, _capacity, _pre_gap + _size,
            right.getr(), right.size(), 0,
            right.size());
        _pre_gap = _pre_gap - left.size();
        _size = left.size() + _size + right.size();

        dbg_self_test();
    }

    void Blob::clear() {
        if (_buffer != nullptr) {
            delete[] _buffer;
            _buffer = nullptr;
        }

        _pre_gap = 0;
        _size = 0;
        _capacity = 0;
    }

    Blob::~Blob() {
        clear();
    }
}