//
// Created by henryco on 12/03/25.
//

#ifndef EX_LIMBO_DATA_ARRAY_MAP_H
#define EX_LIMBO_DATA_ARRAY_MAP_H

#include "array.h"

namespace ex::data {

    template<typename key_t, typename val_t>
    struct array_map { // linear time map, perfect for small data sets ( < ~1000) due to cache locality

        struct entry {
            key_t key;
            val_t val;
        };

        array<key_t> key_entries;
        array<val_t> val_entries;

        const int &capacity = key_entries.capacity;
        const int &size     = key_entries.size;

        ~array_map() = default;

        array_map() = default;

        array_map(const allocator &allocator_) :
            key_entries(allocator_), val_entries(allocator_) {
        }

        array_map(int capacity) :
            key_entries(capacity), val_entries(capacity) {
        }

        array_map(int capacity, const allocator &allocator_) :
            key_entries(capacity, allocator_), val_entries(capacity, allocator_) {
        }

        array_map(const array_map &src) :
            key_entries(src.key_entries), val_entries(src.val_entries) {
        }

        array_map(array_map &&src) noexcept :
            key_entries(static_cast<array<key_t> &&>(src.key_entries)),
            val_entries(static_cast<array<val_t> &&>(src.val_entries)) {
        }

        array_map &operator = (const array_map &other) {
            if (this == &other)
                return *this;
            key_entries = other.key_entries;
            val_entries = other.val_entries;
            return *this;
        }

        array_map &operator = (array_map &&other) noexcept {
            if (this == &other)
                return *this;
            key_entries = static_cast<array<key_t> &&>(other.key_entries);
            val_entries = static_cast<array<val_t> &&>(other.val_entries);
            return *this;
        }

        void put(const entry &entry) {
            put(entry.key, entry.val);
        }

        void put(const key_t &key, const val_t &element) {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key) {
                    val_entries[i] = element;
                    return;
                }
            }
            key_entries.push(key);
            val_entries.push(element);
        }

        val_t &operator [] (const key_t &key) {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key)
                    return val_entries[i];
            }
            key_entries.push(key);
            val_entries.push(val_t());
            return val_entries[val_entries.size - 1];
        }

        const val_t &operator [] (const key_t &key) const {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key)
                    return val_entries[i];
            }
            abort();
        }

        int index_at(const key_t &key) const {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key)
                    return i;
            }
            abort();
        }

        val_t &at(const key_t &key) {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key)
                    return val_entries[i];
            }
            abort();
        }

        const val_t &at(const key_t &key) const {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key)
                    return val_entries[i];
            }
            abort();
        }

        const val_t &at_index(const int &index) const {
            return val_entries[index];
        }

        val_t &at_index(const int &index) {
            return val_entries[index];
        }

        val_t at_or_default(const key_t &key, const val_t &default_value = val_t()) const {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key)
                    return val_entries[i];
            }
            return default_value;
        }

        bool contains(const key_t &key) const {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key)
                    return true;
            }
            return false;
        }

        void remove(const key_t &key) {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key) {
                    key_entries.swap_remove(i);
                    val_entries.swap_remove(i);
                    return;
                }
            }
        }

        void collapse(const key_t &key) {
            for (int i = 0; i < key_entries.size; ++i) {
                if (key_entries[i] == key) {
                    key_entries.collapse(i);
                    val_entries.collapse(i);
                    return;
                }
            }
        }

        void remove_at_pos(const int &idx) {
            if (idx >= key_entries.size || idx < 0)
                return;
            key_entries.swap_remove(idx);
            val_entries.swap_remove(idx);
        }

        void collapse_at_pos(const int &idx) {
            if (idx >= key_entries.size || idx < 0)
                return;
            key_entries.collapse(idx);
            val_entries.collapse(idx);
        }

        bool empty() const {
            return size <= 0;
        }

        void clean() {
            key_entries.clean();
            val_entries.clean();
        }

        void reserve(const int n) {
            if (n <= capacity)
                return;
            key_entries.reserve(n);
            val_entries.reserve(n);
        }
    };

}

#endif //EX_LIMBO_DATA_ARRAY_MAP_H
