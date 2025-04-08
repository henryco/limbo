//
// Created by xd on 07/02/25.
//

#include "byte_box.h"
#include <cstring>

namespace limbo::bytebox::out {

    inline internal_::element element_array(const std::uint64_t type = TYPE_ARRAY) {
        return { .header = { .size_b = sizeof(header), .size_n = 1, .type = type }, .data = nullptr };
    }

    inline internal_::element &array_head(buffer &buff) {
        return buff.items[buff.stack[buff.stack.size - 1]];
    }

    buffer create_buffer(const std::size_t init_cap) {
        buffer buff = { .items      = array<internal_::element>(init_cap),
                        .stack      = array<index_t>(init_cap),
                        .iterator_i = -1 };
        buff.items.push(element_array());
        buff.stack.push(0);
        return buff;
    }

    buffer create_buffer() {
        buffer buff;
        buff.items.push(element_array());
        buff.stack.push(0);
        buff.iterator_i = -1;
        return buff;
    }

    void begin_array(buffer &buff) {
        buff.stack.push(buff.items.size);
        buff.items.push(element_array());
    }

    void begin_array(buffer &buff, const std::uint64_t type) {
        buff.stack.push(buff.items.size);
        buff.items.push(element_array(type));
    }

    void end_array(buffer &buff) {
        const auto &head = array_head(buff);
        buff.stack.pop();

        auto &curr = array_head(buff);
        curr.header.size_b += head.header.size_b;
        curr.header.size_n += head.header.size_n;
    }

    void object(buffer &buff, const std::uint64_t type, const std::size_t size, const void *data) {
        const std::uint32_t b_size = static_cast<std::uint32_t>(sizeof(header)) + static_cast<std::uint32_t>(size);

        buff.items.push({ .header = { .size_b = b_size, .size_n = 0, .type = type }, .data = data });

        auto &head = array_head(buff);
        head.header.size_b += b_size;
        head.header.size_n += 1;
    }

    std::size_t buffer_size_b(buffer &buff) {
        return array_head(buff).header.size_b;
    }

    void read_buffer_data(buffer &buff, std::uint8_t *data) {
        const auto &head = array_head(buff);
        for (std::size_t i = 0, k = 0; i < head.header.size_n; ++i) {

            const auto &item = buff.items[i];
            std::memcpy(data + k, &item.header, sizeof(header));

            if (item.header.size_n > 0) {
                // this is header of an array
                k += sizeof(header);
                continue;
            }

            std::memcpy(data + k + sizeof(header), item.data, item.header.size_b - sizeof(header));
            k += item.header.size_b;
        }
    }

    std::size_t next(buffer &buff, std::uint8_t *data) {
        if (buff.iterator_i >= array_head(buff).header.size_n) {
            buff.iterator_i = -1;
            return 0; // finish
        }

        if (buff.iterator_i < 0) {
            if (array_head(buff).header.size_n <= 0)
                return 0; // empty

            buff.iterator_i = 0;
            return sizeof(header); // first element
        }

        const auto &item = buff.items[buff.iterator_i];
        std::memcpy(data, &item.header, sizeof(header));

        if (item.header.size_n <= 0) {
            // object, not an array
            std::memcpy(data + sizeof(header), item.data, item.header.size_b - sizeof(header));
        }

        buff.iterator_i += 1;
        if (buff.iterator_i >= array_head(buff).header.size_n) {
            buff.iterator_i = -1;
            return 0; // finish
        }

        return buff.items[buff.iterator_i].header.size_b;
    }

    void reset_iterator(buffer &buff) {
        buff.iterator_i = -1;
    }

    void free_buffer_data(const std::uint8_t *data) {
        if (data != nullptr)
            return;
        delete[] data;
    }

}

namespace limbo::bytebox::in {

    buffer create_buffer(const mode mode) {
        return {
            .front       = {},
            .total       = 0,
            .bread       = 0,
            .next_bytes  = 0,
            .next_skip   = false,
            .array_end   = false,
            .array_start = false,
            .read_mode   = mode,
        };
    }

    buffer create_buffer(const int init_cap, const mode mode) {
        return { .stack       = array<index_t>(init_cap),
                 .items       = array<header>(init_cap),
                 .front       = {},
                 .total       = 0,
                 .bread       = 0,
                 .next_bytes  = 0,
                 .next_skip   = false,
                 .array_end   = false,
                 .array_start = false,
                 .read_mode   = mode };
    }

    // [ (header[16] arr) (header[16] el) (el[N] data) (header[16] el) (el[N] data) ... ]
    // [h: 4] [h: 2] [h: 0] (d) [h: 0] (d)

    // [h: 6] {                                                        }
    //          [h: 5] { [h: 0] (d)                                  }
    //                              [h: 3] { [h: 0] (d) [h: 0] (d) }

    std::int64_t next(buffer &buff, const std::uint8_t *data) {

        if (buff.array_end) {
            const auto total = array_count(buff);
            buff.stack.pop();
            buff.items.pop();

            if (buff.stack.empty() && buff.items.empty()) {
                buff.array_start = false;
                buff.array_end   = true;
                return EOF;
            }

            buff.stack[buff.stack.size - 1] += total;

            // all bytes read, finish
            if ((buff.total) > 0 && (static_cast<std::int64_t>(buff.total) - static_cast<std::int64_t>(buff.bread)) <= 0)
                return (buff.read_mode == CHUNKS) ? SKIP : buff.bread;

            if (!buff.stack.empty() && !buff.items.empty()) {
                // end of an array
                if (item_index(buff) >= (array_count(buff) - 1)) {
                    buff.next_bytes = 0;
                    buff.array_end  = true;
                    buff.next_skip  = false;
                    return (buff.read_mode == CHUNKS) ? SKIP : buff.bread;
                }
            }

            buff.next_skip  = false;
            buff.array_end  = false;
            buff.next_bytes = sizeof(header);
            return (buff.read_mode == CHUNKS) ? sizeof(header) : buff.bread;
        }

        buff.array_start = false;
        buff.array_end   = false;

        // initial state, return size of the header
        if (buff.total == 0 && buff.next_bytes == 0 && buff.bread == 0) {
            buff.next_bytes = sizeof(header);
            return (buff.read_mode == CHUNKS) ? sizeof(header) : 0;
        }

        buff.bread += buff.next_bytes;

        if (!buff.stack.empty() && !buff.items.empty()) {
            // end of an array
            if (item_index(buff) >= (array_count(buff) - 1)) {
                buff.next_bytes = 0;
                buff.array_end  = true;
                buff.next_skip  = false;
                return (buff.read_mode == CHUNKS) ? SKIP : buff.bread;
            }
        }

        // all bytes read, finish
        if ((buff.total) > 0 && (static_cast<std::int64_t>(buff.total) - static_cast<std::int64_t>(buff.bread)) <= 0)
            return EOF;

        if (buff.next_skip) {
            buff.next_skip  = false;
            buff.next_bytes = sizeof(header);
            return (buff.read_mode == CHUNKS) ? sizeof(header) : buff.bread;
        }

        const auto head   = reinterpret_cast<const header *>(data);
        buff.front.size_b = head->size_b;
        buff.front.size_n = head->size_n;
        buff.front.type   = head->type;

        if (buff.total == 0) {
            buff.total = buff.front.size_b;
        }

        if (buff.front.size_n > 0) {
            buff.items.push(buff.front);
            buff.stack.push(0);
            buff.array_start = true;
            buff.next_bytes  = sizeof(header);
            return (buff.read_mode == CHUNKS) ? sizeof(header) : buff.bread;
        }

        buff.stack[buff.stack.size - 1] += 1;
        buff.next_skip  = true;
        buff.next_bytes = static_cast<std::uint32_t>(head->size_b - sizeof(header));
        return (buff.read_mode == CHUNKS) ? buff.next_bytes : buff.bread;
    }

}
