
#include <assert.h>
#include <cstdint>
#include <memory>
#include <string>

#include "copy_protection.h"

namespace NATBuster::Common::Utils {
    class Blob : Utils::NonCopyable
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
        
        inline uint32_t get_end_gap() const {
            return _capacity - _offset - _size;
        }

        //Consume a buffer, holding the data, but with pre and post gaps
        Blob(uint8_t* buffer_consume, uint32_t buffer_size, uint32_t data_size, uint32_t data_offset = 0);

        //Consume a buffer, holding the data with no gaps
        Blob(uint8_t* buffer_consume, uint32_t buffer_size);

    public:
        Blob();
        Blob(Blob&& other);
        Blob& operator=(Blob&& other);

        //Factory to consume an existing buffer
        static Blob factory_consume(uint8_t* data_consume, uint32_t len);
        static Blob factory_copy(uint8_t* buffer_copy, uint32_t buffer_size, uint32_t pre_gap = 0, uint32_t end_gap = 0);
        static Blob factory_string(const std::string& str);

        static Blob concat(std::initializer_list<Blob*> list);

        void grow(uint32_t min_pre_gap, uint32_t min_end_gap, bool smart = true);
        inline void grow_end_gap(uint32_t min_end_gap, bool smart = true) {
            grow(0, min_end_gap, smart);
        }
        inline void grow_pre_gap(uint32_t min_pre_gap, bool smart = true) {
            grow(min_pre_gap, 0, smart);
        }

        //Start and end coordinates wrt old buffer indices
        void resize(int32_t new_start, uint32_t new_end);

        void add_blob_after(const Blob& other);
        void add_blob_before(const Blob& other);
        void sandwich(const Blob& left, const Blob& right);

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

        void clear();

        ~Blob();
    };

    //To pass a blob for read/writing
    class BlobView {
        Blob* _blob;

        uint32_t _start;
        uint32_t _len;

    public:
        BlobView(Blob* blob, uint32_t start, uint32_t len) : _blob(blob), _start(start), _len(len) {

        }

        void resize(uint32_t new_len) {
            uint32_t new_end = _start + new_len;

            if (_blob->size() <= new_end) {
                _blob->grow_end_gap(new_end - _blob->size(), true);
            }

            _len = new_len;
        }

        uint8_t* get() {
            return _blob->get() + _start;
        }

        const uint8_t* get() const {
            return _blob->get() + _start;
        }

        uint32_t size() const {
            return _len;
        }
    };
}