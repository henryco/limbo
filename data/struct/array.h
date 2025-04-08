//
// Created by henryco on 12/03/25.
//

#ifndef EX_LIMBO_DATA_ARRAY_H
#define EX_LIMBO_DATA_ARRAY_H

#include "../alloc/datalloc.h"
#include <cstring>

namespace ex::data {

    template<typename val_t>
    struct array {

        using comparator_fn = int8_t (*)(const val_t *a, const val_t *b);
        static int8_t default_comparator(const val_t *a, const val_t *b) {
            return (*a > *b) - (*a < *b);
        }

        allocator allocator_;

        val_t *data     = nullptr;
        int    size     = 0;
        int    capacity = 0;

        array() {
        }

        array(const allocator &allocator_) {
            this->allocator_ = allocator_;
        }

        ~array() {
            free_(this, allocator_, data);
            data = nullptr;
        }

        array(const int &capacity) : capacity(capacity) {
            data = malloc_<val_t>(this, allocator_, capacity);
        }

        array(const int &capacity, const allocator &allocator_) : capacity(capacity) {
            this->allocator_ = allocator_;
            data = malloc_<val_t>(this, this->allocator_, capacity);
        }

        val_t &operator[](const int &index) {
            return data[index];
        }

        const val_t &operator[](const int &index) const {
            return data[index];
        }

        val_t &front() {
            return data[0];
        }

        const val_t &front() const {
            return data[0];
        }

        val_t &back() {
            return data[size - 1];
        }

        const val_t &back() const {
            return data[size - 1];
        }

        void clean() {
            free_(this, allocator_, data);
            this->data = nullptr;
            size     = 0;
            capacity = 0;
        }

        void reserve(const int n) {
            if (n <= capacity)
                return;

            if (data == nullptr)
                data = malloc_<val_t>(this, allocator_, n);
            else
                data = realloc_<val_t>(this, allocator_, data, n);

            capacity = n;
        }

        int binary_search(const val_t &value, const comparator_fn compare = &default_comparator) const {
            if (data == nullptr)
                return -1;

            //[n: 2]
            // i: 0 1 2 3 4 5 6 7 8
            // d: 0 1 4 5 6 7 7 8 9
            //       *            ^  8
            //       |    ^          4
            //       |^              2
            //      ^|               1
            //       |^              1

            int index = size - 1;
            int width = size;
            while (true) {
                const int8_t r = compare(&value, &data[index]);

                if (r == 0)
                    return index;

                width = (width / 2) > 0
                    ? (width / 2)
                    : 1;

                if (r < 0) index -= width;
                else       index += width;

                if (index >= size)
                    return size;

                if (index < 0)
                    return -1;

                if (width == 1) {
                    const int8_t rr = compare(&value, &data[index]);
                    if (rr == 0)
                        return index;

                    if (rr < 0 && r > 0)
                        return index;

                    if (rr > 0 && r < 0)
                        return index + 1;
                }
            }
        }

        int sort_insert(const val_t &value, const comparator_fn compare = &default_comparator) {
            const int i = binary_search(value, compare);

            if (i >= size) {
                push(value);
                return size - 1;
            }

            if (i <= 0) {
                insert(0, value);
                return 0;
            }

            insert(i, value);
            return i;
        }

        int sort_insert_once(const val_t &value, const comparator_fn compare = &default_comparator) {
            const int i = binary_search(value, compare);

            if (i >= size) {
                push(value);
                return size - 1;
            }

            if (i <= 0) {
                insert(0, value);
                return 0;
            }

            if (compare(&data[i], &value) == 0)
                return i;

            insert(i, value);
            return i;
        }

        void insert(const int index, const val_t &value) {
            if (size >= capacity) {
                capacity = capacity > 0 ? (2 * capacity) : 2;

                if (data == nullptr)
                    data = malloc_<val_t>(this, allocator_, capacity);
                else
                    data = realloc_<val_t>(this, allocator_, data, capacity);
            }
            ::memmove(data + index + 1, data + index, (size - index) * sizeof(val_t));
            data[index] = value;
            size++;
        }

        void push(const val_t &value) {
            if (size >= capacity) {
                capacity = capacity > 0 ? (2 * capacity) : 2;

                if (data == nullptr)
                    data = malloc_<val_t>(this, allocator_, capacity);
                else
                    data = realloc_<val_t>(this, allocator_, data, capacity);
            }
            data[size++] = value;
        }

        void collapse(const int index) {
            if (index >= size - 1) {
                size--;
                return;
            }

            // 0 1 2 3 4 5 6 : 7
            // x x x x _ x x

            // dst: 4
            // src: 5
            //   n: 2 = size(7) - index(4) - 1

            ::memmove(data + index, data + index + 1, (size - index - 1) * sizeof(val_t));
            size--;
        }

        void pop() {
            size = (size > 0) ? (size - 1) : 0;
        }

        bool empty() const {
            return size <= 0;
        }

        bool contains(const val_t &value) const {
            for (int i = 0; i < size; ++i) {
                if (data[i] == value)
                    return true;
            }
            return false;
        }

        void remove_element(const val_t &value) {
            for (int i = 0; i < size; ++i) {
                if (data[i] == value) {
                    swap_remove(i);
                    return;
                }
            }
        }

        void collapse_element(const val_t &value) {
            for (int i = 0; i < size; ++i) {
                if (data[i] == value) {
                    collapse(i);
                    return;
                }
            }
        }

        int index_of(const val_t &value) const {
            for (int i = 0; i < size; ++i) {
                if (data[i] == value)
                    return i;
            }
            return -1;
        }

        void swap(const int index_1, const int index_2) {
            const val_t tmp = data[index_1];
            data[index_1]       = data[index_2];
            data[index_2]       = tmp;
        }

        void swap_remove(const int &index) {
            const val_t tmp = data[index];
            data[index]         = data[size - 1];
            data[size - 1]      = tmp;
            size                = (size > 0) ? (size - 1) : 0;
        }

        array(const array &other) : size(other.size), capacity(other.capacity) {
            if (other.capacity <= 0) {
                data = nullptr;
                return;
            }

            allocator_ = other.allocator_;

            data = malloc_<val_t>(this, allocator_, other.capacity);

            if (other.size <= 0)
                return;

            ::memcpy(data, other.data, other.size * sizeof(val_t));
        }

        array(array &&other) noexcept : size(other.size), capacity(other.capacity) {
            this->data = other.data;
            other.data = nullptr;
        }

        array &operator = (const array &other) {
            if (this == &other)
                return *this;

            free_(this, allocator_, data);
            data = nullptr;

            size       = other.size;
            capacity   = other.capacity;
            allocator_ = other.allocator_;

            if (other.capacity <= 0) {
                data = nullptr;
                return *this;
            }

            data = malloc_<val_t>(this, other.allocator_, other.capacity);

            if (other.size <= 0)
                return *this;

            ::memcpy(data, other.data, other.size * sizeof(val_t));
            return *this;
        }

        array &operator = (array &&other) noexcept {
            if (this == &other)
                return *this;

            free_(this, allocator_, data);
            data = nullptr;

            this->data       = other.data;
            this->size       = other.size;
            this->capacity   = other.capacity;
            this->allocator_ = other.allocator_;
            other.data       = nullptr;

            return *this;
        }

        struct iterator {

            val_t *data_;

            iterator(val_t *data) :
                data_(data) {
            }

            val_t &operator * () {
                return *data_;
            }

            const val_t &operator * () const {
                return *data_;
            }

            iterator &operator ++ () {
                data_ = data_ + 1;
                return *this;
            }

            iterator &operator -- () {
                data_ = data_ - 1;
                return *this;
            }

            iterator operator ++ (int) {
                iterator temp = *this;
                data_ = data_ + 1;
                return temp;
            }

            iterator operator -- (int) {
                iterator temp = *this;
                data_ = data_ - 1;
                return temp;
            }

            bool operator != (const iterator &other) const {
                return data_ != other.data_;
            }

            bool operator == (const iterator &other) const {
                return data_ == other.data_;
            }
        };

        iterator begin() const {
            return iterator(data);
        }

        iterator end() const {
            return iterator(data + size);
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

#endif //EX_LIMBO_DATA_ARRAY_H
