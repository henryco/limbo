//
// Created by henryco on 12/03/25.
//

#ifndef EX_LIMBO_DATA_BUFFER_H
#define EX_LIMBO_DATA_BUFFER_H

#include "../alloc/datalloc.h"
#include <cstring>

namespace ex::data {

    template<typename element_t>
    struct buffer {

        allocator allocator_;

        element_t *data = nullptr;
        long       size = 0;

        buffer() {
        }

        buffer(const allocator &allocator_) : allocator_(allocator_) {
        }

        buffer(const long &size) : size(size) {
            this->data = malloc_<element_t>(this, allocator_, size);
        }

        buffer(const long &size, const allocator &allocator_) : allocator_(allocator_), size(size) {
            this->data = malloc_<element_t>(this, allocator_, size);
        }

        ~buffer() {
            free_(this, allocator_, data);
            this->data = nullptr;
            this->size = 0;
        }

        void allocate(const long &size) {
            if (size <= this->size)
                return;

            if (this->size <= 0 || this->data == nullptr) {
                this->data = malloc_<element_t>(this, allocator_, size);
                this->size = size;
            }

            else {
                this->data = realloc_<element_t>(this, allocator_, this->data, size);
                this->size = size;
            }
        }

        buffer(const buffer &other) : allocator_(other.allocator_), size(other.size) {
            if (other.size <= 0) {
                data = nullptr;
                return;
            }

            data = malloc_<element_t>(this, other.allocator_, other.size);
            memcpy(data, other.data, other.size * sizeof(element_t));
        }

        buffer(buffer &&other) noexcept : allocator_(other.allocator_), size(other.size) {
            this->data = other.data;
            other.data = nullptr;
        }

        buffer &operator = (const buffer &other) {
            if (this == &other)
                return *this;

            free_(this, allocator_, data);
            data = nullptr;

            allocator_ = other.allocator_;
            size       = other.size;

            if (other.size <= 0) {
                data = nullptr;
                return *this;
            }

            data = malloc_<element_t>(this, other.allocator_, other.size);
            memcpy(data, other.data, other.size * sizeof(element_t));

            return *this;
        }

        buffer &operator = (buffer &&other) noexcept {
            if (this == &other)
                return *this;

            free_(this, allocator_, data);
            data = nullptr;

            this->allocator_ = other.allocator_;
            this->size       = other.size;
            this->data       = other.data;
            other.data       = nullptr;

            return *this;
        }

        template <typename T>
        T *malloc_(void *, allocator &allocator, const std::size_t size_n) {
            return static_cast<T *>(allocator.malloc(&allocator, size_n * sizeof(T), sizeof(T)));
        }

        template <typename T>
        T *realloc_(void *, allocator &allocator, T *const ptr, const std::size_t size_n) {
            return static_cast<T *>(allocator.realloc(&allocator, ptr, size_n * sizeof(T)));
        }

        static void free_(void *, allocator &allocator, void *ptr) {
            if (allocator.free != nullptr && ptr != nullptr)
                allocator.free(&allocator, ptr);
        }
    };

}

#endif //EX_LIMBO_DATA_BUFFER_H
