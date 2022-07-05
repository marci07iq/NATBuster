#pragma once

#include <assert.h>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "copy_protection.h"

namespace NATBuster::Common::Utils {
    class _ConstBlobView;
    typedef const _ConstBlobView ConstBlobView;
    class BlobView;

    class Blob;
    class BlobSliceView;
    class ConstBlobSliceView;

    class _ConstBlobView {
    public:
        _ConstBlobView();

        virtual const uint8_t* getr() const = 0;

        virtual const uint32_t size() const = 0;

        const ConstBlobSliceView cslice(uint32_t start, uint32_t len) const;

        //Part of this BlobView before the split
        const ConstBlobSliceView cslice_left(uint32_t split) const;
        //Part of this BlobView after the split
        const ConstBlobSliceView cslice_right(uint32_t split) const;
    };

    class BlobView : public ConstBlobView {
    public:
        BlobView();

        virtual void resize(uint32_t new_len) = 0;

        virtual void resize_min(uint32_t min_len) = 0;

        virtual uint8_t* getw() = 0;

        BlobSliceView slice(uint32_t start, uint32_t len);

        //Part of this BlobView before the split
        BlobSliceView slice_left(uint32_t split);
        //Part of this BlobView after the split
        BlobSliceView slice_right(uint32_t split);
    };

    class Blob : public BlobView
    {
        uint8_t* _buffer;
        uint32_t _pre_gap;
        uint32_t _capacity;
        uint32_t _size;

        friend class BlobView;

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
        Blob();
        Blob(const Blob& other) = delete;
        Blob(Blob&& other);
        Blob& operator=(Blob&& other);
        Blob& operator=(const Blob&) = delete;

        //Factory to consume an existing buffer
        static Blob factory_consume(uint8_t* data_consume, uint32_t len);
        //Factory to copy a buffer, and optionally add a gap before/after
        static Blob factory_copy(uint8_t* buffer_copy, uint32_t buffer_size, uint32_t pre_gap = 0, uint32_t end_gap = 0);
        //Factory from std::string. Doesn't stop at null, termination, stops at str.size()
        static Blob factory_string(const std::string& str);

        static Blob concat(std::initializer_list<ConstBlobView*> list, uint32_t pre_gap = 0, uint32_t end_gap = 0);

        //Pre-allocate space before and after the current active area
        void grow_gap(uint32_t min_pre_gap, uint32_t min_end_gap, bool smart = true);
        inline void grow_end_gap(uint32_t min_end_gap, bool smart = true) {
            grow_gap(0, min_end_gap, smart);
        }
        inline void grow_pre_gap(uint32_t min_pre_gap, bool smart = true) {
            grow_gap(min_pre_gap, 0, smart);
        }

        //Start and end coordinates wrt old buffer indices
        void resize(int32_t new_start, uint32_t new_end);
        //End coordinates wrt old buffer indices
        inline void resize(uint32_t new_end) {
            resize(0, new_end);
        }
        //End coordinates wrt old buffer indices
        inline void resize_min(uint32_t min_len) {
            if (_size < min_len) {
                grow_end_gap(min_len - _size);
                _size = min_len;
            }
        }

        void add_blob_after(const ConstBlobView& other);
        void add_blob_before(const ConstBlobView& other);
        void sandwich(const ConstBlobView& left, const BlobView& right);

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

        uint8_t* getw() {
            return _blob->getw() + _start;
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
            assert(_start + _len <= blob->size());

        }

        //Full
        ConstBlobSliceView(const BlobView* blob) : ConstBlobSliceView(blob, 0, blob->size()) {

        }

        const uint8_t* getr() const {
            return _blob->getr() + _start;
        }

        const uint32_t size() const {
            return _len;
        }
    };
}