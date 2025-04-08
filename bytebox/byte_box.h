//
// Created by xd on 07/02/25.
//
// ReSharper disable CppNonExplicitConvertingConstructor

#ifndef BYTE_BOX_H
#define BYTE_BOX_H

#include <cstdint>
#include <cstdlib>

namespace limbo::bytebox {

    #define TYPE_ARRAY   0
    #define SKIP         0
    #define EOF        (-1)

    using index_t = std::uint32_t;
    using type__t = std::uint64_t;

    struct alignas(16) header {
        std::uint32_t size_b;
        std::uint32_t size_n;
        std::uint64_t type;
    };

    template<typename element>
    struct array {
        element *data     = nullptr;
        index_t  size     = 0;
        index_t  capacity = 0;

        array() = default;

        ~array() {
            if (data == nullptr)
                return;
            free(data);
        }

        array(const index_t capacity) {
            this->capacity = capacity;
            this->data     = static_cast<element *>(malloc(capacity * sizeof(element)));
        }

        element &operator [] (const index_t index) {
            return data[index];
        }

        const element &operator [] (const index_t index) const {
            return data[index];
        }

        void push(const element &value) {
            if (size >= capacity) {
                capacity = capacity > 0 ? (2 * capacity) : 2;
                data     = static_cast<element *>(realloc(data, capacity * sizeof(element)));
            }
            data[size++] = value;
        }

        void pop() {
            size = (size > 0) ? (size - 1) : 0;
        }

        bool empty() const {
            return size <= 0;
        }
    };

    namespace internal_ {
        struct element {
            header      header;
            const void *data;
        };
    }

    namespace out {

        struct buffer {
            array<internal_::element> items;
            array<index_t>            stack;
            std::int32_t              iterator_i;
        };

        buffer create_buffer();

        buffer create_buffer(std::size_t init_cap);

        /**
         * Begin data array
         * @param buff buffer instance
         */
        void begin_array(buffer &buff);

        /**
         * Begin data array
         * @param buff buffer instance
         * @param type type of array
         */
        void begin_array(buffer &buff, type__t type);

        /**
         * End data array
         * @param buff buffer instance
         */
        void end_array(buffer &buff);

        /**
         * Write object to buffer
         * @param buff buffer instance
         * @param type type of object
         * @param size size of an object in bytes
         * @param data object data
         */
        void object(buffer &buff, type__t type, std::size_t size, const void *data);

        /**
         * @param buff buffer instance
         * @return size of the buffered data
         */
        std::size_t buffer_size_b(buffer &buff);

        /**
         * Read whole buffer data
         * @param buff buffer instance
         * @param out array to read data
         */
        void read_buffer_data(buffer &buff, std::uint8_t *out);

        /**
         * Read next data chunk
         * @param buff buffer instance
         * @param out array to read data
         * @return size of the next data chunk
         */
        std::size_t next(buffer &buff, std::uint8_t *out);

        /**
         * Resets output iterator,
         * see <b>std::size_t next(buffer &, std::uint8_t *)</b>
         * @param buff buffer instance
         */
        void reset_iterator(buffer &buff);
    }

    namespace in {

        enum mode: bool {
            // chunk mode, useful when reading data in chunks
            CHUNKS = 0,

            // offset mode
            OFFSET = 1
        };

        struct buffer {
            array<index_t> stack;
            array<header>  items;

            header  front;
            index_t total;
            index_t bread;

            index_t next_bytes;
            bool    next_skip;
            bool    array_end;
            bool    array_start;
            mode    read_mode;
        };

        buffer create_buffer(mode mode = CHUNKS);

        buffer create_buffer(int init_cap, mode mode = CHUNKS);

        /**
         * @param buff buffer instance
         * @param data binary data to read
         * @return size of the next chunk or total offset
         */
        std::int64_t next(buffer &buff, const std::uint8_t *data);

        inline bool begin_array(const buffer &buff) {
            return buff.array_start;
        }

        inline bool end_array(const buffer &buff) {
            return buff.array_end;
        }

        inline type__t array_type(const buffer &buff) {
            return buff.items[buff.items.size - 1].type;
        }

        inline index_t array_size(const buffer &buff) {
            return buff.items[buff.items.size - 1].size_b;
        }

        inline index_t array_count(const buffer &buff) {
            return buff.items[buff.items.size - 1].size_n;
        }

        inline std::int64_t next_object(const buffer &buff) {
            return buff.next_skip ? (buff.front.size_b - sizeof(header)) : 0;
        }

        inline type__t item_type(const buffer &buff) {
            return buff.front.type;
        }

        inline index_t item_size(const buffer &buff) {
            return buff.front.size_b;
        }

        inline index_t item_index(const buffer &buff) {
            return buff.stack[buff.stack.size - 1];
        }

        inline std::size_t offset(const buffer &buff) {
            return buff.bread;
        }

    }

}

#endif // BYTE_BOX_H
