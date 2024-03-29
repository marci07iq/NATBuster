#pragma once

#include <assert.h>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <map>
#include <set>

#include "copy_protection.h"

namespace NATBuster::Utils {
    class _ConstBlobView;
    typedef const _ConstBlobView ConstBlobView;
    class BlobView;

    class Blob;
    class BlobSliceView;
    class ConstBlobSliceView;

    class _ConstBlobView : Utils::NonCopyable {
    protected:
        _ConstBlobView();
    public:
        //Get read only buffer. Index 0 is at the start of the view
        virtual const uint8_t* getr() const = 0;

        //Get size of the buffer that can be safely read/written
        virtual const uint32_t size() const = 0;

        //Create a new cosnt slice view, looking at the segment of this
        //New view must me inside the active region of this view
        ConstBlobSliceView cslice(uint32_t start, uint32_t len) const;

        //Part of this BlobView before the split point
        //Non incusive
        ConstBlobSliceView cslice_left(uint32_t split) const;
        //Part of this BlobView after the split point
        //Inclusive
        ConstBlobSliceView cslice_right(uint32_t split) const;
    };

    bool operator==(const ConstBlobView& lhs, const ConstBlobView& rhs);

    bool operator!=(const ConstBlobView& lhs, const ConstBlobView& rhs);

    std::strong_ordering operator<=>(const ConstBlobView& lhs, const ConstBlobView& rhs);


    struct BlobSorter {
        //To enable searching in maps by any key
        using is_transparent = void;

        bool operator()(const ConstBlobView& lhs, const ConstBlobView& rhs) const {
            return lhs < rhs;
        }
    };

    template<typename V>
    using BlobIndexedMap = std::map<Blob, V, BlobSorter>;
    template<typename V>
    using BlobSet = std::set<Blob, BlobSorter>;

    class BlobView : public ConstBlobView {
    protected:
        BlobView();
    public:
        //Resize the active region of this view.
        //Resizes underlying structure to make it larger if needed,
        //doesn't shrink underlying structure
        virtual void resize(uint32_t new_len) = 0;

        //Set the minimum size of this view. Increases the size of the underlying view if needed
        //Doesnt shrink this or the underyling view
        virtual void resize_min(uint32_t min_len) = 0;

        //Set the minimum capacity of this view
        //Increases the capacity of the underyling view if needed
        //Capacity is the size to which the view can be resized without needin to do any new memory alloc/copy
        virtual void capacity_min(uint32_t min_cap) = 0;

        //get writeable buffer
        virtual uint8_t* getw() = 0;

        virtual void copy_from(const ConstBlobView& src, uint32_t dst_offset = 0) = 0;

        //Create a slice, realative to this view.
        //Can be outside of the current active area.
        //Will resizte this view, so that the returned view is always inside
        BlobSliceView slice(uint32_t start, uint32_t len);

        //Part of this BlobView before the split point
        BlobSliceView slice_left(uint32_t split);
        //Part of this BlobView after the split point
        BlobSliceView slice_right(uint32_t split);
    };

    class Blob : public BlobView
    {
        uint8_t* _buffer;
        uint32_t _pre_gap;
        uint32_t _capacity;
        uint32_t _size;

        friend class BlobView;

    public:
        //Allocate and zero out a buffer
        static uint8_t* alloc(uint32_t size) {
            uint8_t* res = new uint8_t[size];
            memset(res, 0, size);
            return res;
        }


        //Copy between buffers with given maxium capacity
        //Checking for overrun
        inline static void bufcpy(uint8_t* dst, uint32_t dst_capacity, uint32_t dst_offset, const uint8_t* src, uint32_t src_capacity, uint32_t src_offset, uint32_t data_size) {
            assert(dst_offset + data_size <= dst_capacity);
            assert(src_offset + data_size <= src_capacity);

            memcpy(dst + dst_offset, src + src_offset, data_size);
        }

    private:
        inline void dbg_self_test() const {
#ifndef NDEBUG
            assert(_buffer != nullptr || (_capacity == 0 && _size == 0));
            assert(_pre_gap <= _capacity);
            assert(_size <= _capacity);
            assert(_pre_gap + _size <= _capacity);
#endif
        }

        inline uint32_t get_end_gap() const {
            return _capacity - _pre_gap - _size;
        }

        //Consume a buffer, holding the data, but with pre and post gaps
        Blob(uint8_t* buffer_consume, uint32_t buffer_size, uint32_t data_size, uint32_t data_pre_gap = 0);

        //Consume a buffer, holding the data with no gaps
        Blob(uint8_t* buffer_consume, uint32_t buffer_size);

    public:
        //Blank, and move ctor
        //No copy.

        Blob();
        Blob(const Blob& other) = delete;
        Blob& operator=(const Blob&) = delete;
        Blob(Blob&& other) noexcept;
        Blob& operator=(Blob&& other) noexcept;

        //Factory to consume an existing buffer
        static Blob factory_empty(uint32_t size, uint32_t pre_gap = 0, uint32_t end_gap = 0);
        //Factory to consume an existing buffer
        static Blob factory_consume(uint8_t* data_consume, uint32_t len);
        //Factory to copy a buffer, and optionally add a gap before/after
        static Blob factory_copy(const uint8_t* buffer_copy, uint32_t buffer_size, uint32_t pre_gap = 0, uint32_t end_gap = 0);
        //Factory to copy a buffer, and optionally add a gap before/after
        static Blob factory_copy(const ConstBlobView& view, uint32_t pre_gap = 0, uint32_t end_gap = 0);
        //Factory from std::string. Doesn't stop at null, termination, stops at str.size()
        static Blob factory_string(const std::string& str);
        //Concat views into one. Optionally add a pre and post gap in capacity.
        static Blob factory_concat(std::initializer_list<ConstBlobView*> list, uint32_t pre_gap = 0, uint32_t end_gap = 0);

        //Pre-allocate space before and after the current active area
        void grow_gap(uint32_t min_pre_gap, uint32_t min_end_gap, bool smart = true);
        inline void grow_end_gap(uint32_t min_end_gap, bool smart = true) {
            grow_gap(0, min_end_gap, smart);
        }
        inline void grow_pre_gap(uint32_t min_pre_gap, bool smart = true) {
            grow_gap(min_pre_gap, 0, smart);
        }

        //Change the active area of the buffer
        //Start and end wrt the current indices
        void resize(int32_t new_start, uint32_t new_end);
        //Change the end of the active area of the buffer
        //End coordinates wrt old buffer indices
        inline void resize(uint32_t new_end) {
            resize(0, new_end);
        }
        //Change the end of the active area of the buffer
        //Can only increase buffer.
        //End coordinates wrt old buffer indices
        inline void resize_min(uint32_t min_len) {
            if (_size < min_len) {
                grow_end_gap(min_len - _size);
                _size = min_len;
            }
        }

        //Set the minimum capacity of the buffer
        inline void capacity_min(uint32_t min_cap) {
            if (_capacity < min_cap) {
                grow_end_gap(min_cap - _size);
            }
        }

        void add_blob_after(const ConstBlobView& other);
        void add_blob_before(const ConstBlobView& other);
        void sandwich(const ConstBlobView& left, const BlobView& right);

        void copy_from(const ConstBlobView& src, uint32_t dst_offset = 0) {
            resize_min(dst_offset + src.size());

            assert(src.size() <= (_size - dst_offset));

            Blob::bufcpy(
                _buffer, _capacity, dst_offset + _pre_gap,
                src.getr(), src.size(), 0, src.size());
        }

        //Keep allocated memory area, but wipe it.
        inline void erase() {
            if (_buffer == nullptr) return;
            memset(_buffer, _capacity, 0);
        }

        inline uint8_t* getw() {
            dbg_self_test();
            if (_buffer == nullptr) return nullptr;
            return _buffer + _pre_gap;
        }
        inline const uint8_t* getr() const {
            dbg_self_test();
            if (_buffer == nullptr) return nullptr;
            return _buffer + _pre_gap;
        }
        inline const uint32_t size() const {
            return _size;
        }

        inline std::string to_string() const {
            return std::string((const char*)getr(), size());
        }

        void clear();

        ~Blob();
    };

    //To pass a blob for read/writing
    class BlobSliceView : public BlobView {
        BlobView* _blob;

        //WRT Blob "active area": 0 at beginning of Blob::get()
        uint32_t _start;
        uint32_t _len;

    public:
        //Sub section
        BlobSliceView(BlobView* blob, uint32_t start, uint32_t len) : _blob(blob), _start(start), _len(len) {
            //Expand underlying buffer to include this
            resize(len);

            //Check 
            assert(_start + _len <= _blob->size());

        }

        //Full
        BlobSliceView(BlobView* blob) : BlobSliceView(blob, 0, blob->size()) {

        }

        //Replace with a copy
        BlobSliceView& operator=(const BlobSliceView& src) {
            _blob = src._blob;
            _start = src._start;
            _len = src._len;
            return *this;
        }

        void resize(uint32_t new_len) {
            uint32_t new_end = _start + new_len;

            _blob->resize_min(new_end);

            _len = new_len;
        }

        void resize_min(uint32_t min_len) {
            if (_len < min_len) {
                resize(min_len);
            }
        }

        void capacity_min(uint32_t min_cap) {
            uint32_t new_cap_end = _start + min_cap;

            _blob->capacity_min(new_cap_end);
        }

        uint8_t* getw() {
            return _blob->getw() + _start;
        }

        virtual void copy_from(const ConstBlobView& src, uint32_t dst_offset = 0) {
            _blob->copy_from(src, _start + dst_offset);
        }

        const uint8_t* getr() const {
            return _blob->getr() + _start;
        }

        const uint32_t size() const {
            return _len;
        }
    };

    //To pass a blob for read/writing
    class ConstBlobSliceView : public ConstBlobView {
        const ConstBlobView* _blob;

        //WRT Blob "active area": 0 at beginning of Blob::get()
        uint32_t _start;
        uint32_t _len;

    public:

        //Sub section
        ConstBlobSliceView(const ConstBlobView* blob, uint32_t start, uint32_t len) : _blob(blob), _start(start), _len(len) {
            //Check
            if (_blob != nullptr) {
                assert(_start + _len <= _blob->size());
            }

        }

        //Empty
        ConstBlobSliceView() : ConstBlobSliceView(nullptr, 0, 0) {

        }

        //Full
        ConstBlobSliceView(const BlobView* blob) : ConstBlobSliceView(blob, 0, blob->size()) {

        }

        //Replace with a copy
        ConstBlobSliceView& operator=(const ConstBlobSliceView& src) {
            _blob = src._blob;
            _start = src._start;
            _len = src._len;

            return *this;
        }

        const uint8_t* getr() const {
            const uint8_t* base = _blob->getr();
            if (base == nullptr) return nullptr;
            return base + _start;
        }

        const uint32_t size() const {
            return _len;
        }
    };

    class PackedBlobWriter : Utils::NonCopyable {
        uint32_t _write_cursor;
        BlobView& _dst;
    public:
        //Appends a packed data record at the end of dst
        PackedBlobWriter(BlobView& dst) : _dst(dst) {
            _write_cursor = dst.size();
        }

        void add_record(const ConstBlobView& data) {
            //Create space
            _dst.resize(_write_cursor + 4 + data.size());

            //Copy data
            _dst.copy_from(data, _write_cursor + 4);

            //Set size field
            *((uint32_t*)(_dst.getw() + _write_cursor)) = data.size();
            _write_cursor = _write_cursor + 4 + data.size();
        }

        BlobSliceView prepare_writeable_record() {
            //Create an empty slice starting after the lenth field
            return _dst.slice(_write_cursor + 4, 0);
        }

        void finish_writeable_record(const BlobSliceView& record) {
            //Set size field
            *((uint32_t*)(_dst.getw() + _write_cursor)) = record.size();
            _write_cursor = _write_cursor + 4 + record.size();
        }
    };

    class PackedBlobReader : Utils::NonCopyable {
        uint32_t _read_cursor;
        const ConstBlobView& _src;
    public:
        //Starts reading at the beginning, till the end of src.
        PackedBlobReader(const ConstBlobView& src) : _src(src), _read_cursor(0) {

        }

        bool next_record(ConstBlobSliceView& new_slice) {
            //Check to make sure record size is in source
            if (_src.size() < _read_cursor + 4) return false;

            uint32_t record_size = *((uint32_t*)(_src.getr() + _read_cursor));

            //Check to make sure record is in source
            //Check ordered to prevent from overflow if attacker controlled record_size is too big.
            if (_src.size() - (4 + _read_cursor) < record_size) return false;

            //Create a slice
            new_slice = _src.cslice(_read_cursor + 4, record_size);

            _read_cursor = _read_cursor + 4 + record_size;

            return true;
        }

        bool eof() {
            assert(_read_cursor <= _src.size());
            return _read_cursor >= _src.size();
        }
    };
}