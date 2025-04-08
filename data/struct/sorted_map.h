//
// Created by henryco on 12/03/25.
//

#ifndef EX_LIMBO_DATA_SORTED_MAP_H
#define EX_LIMBO_DATA_SORTED_MAP_H

#include "array.h"

namespace ex::data {

    template<typename key_t, typename val_t>
    struct sorted_map { // sorted map with binary tree search implementation

        struct entry {
            key_t key;
            val_t val;
        };

        using comparator_fn = int8_t (*)(const key_t *a, const key_t *b);
        static int8_t default_comparator(const key_t *a, const key_t *b) {
            return (*a > *b) - (*a < *b);
        }

        comparator_fn comparator__;
        array<key_t>  key_entries_;
        array<val_t>  val_entries_;

        const int &capacity = key_entries_.capacity;
        const int &size     = key_entries_.size;

        ~sorted_map() = default;

        sorted_map(const comparator_fn compare = &default_comparator):
            comparator__(compare) {
        }

        sorted_map(const allocator &allocator_, const comparator_fn compare = &default_comparator) :
            comparator__(compare), key_entries_(allocator_), val_entries_(allocator_) {
        }

        sorted_map(int capacity, const comparator_fn compare = &default_comparator) :
            comparator__(compare), key_entries_(capacity), val_entries_(capacity) {
        }

        sorted_map(int capacity, const allocator &allocator_, const comparator_fn compare = &default_comparator) :
            comparator__(compare), key_entries_(capacity, allocator_), val_entries_(capacity, allocator_) {
        }

        sorted_map(const sorted_map &src) :
            comparator__(src.comparator__),
            key_entries_(src.key_entries_),
            val_entries_(src.val_entries_) {
        }

        sorted_map(sorted_map &&src) noexcept :
            comparator__(src.comparator__),
            key_entries_(static_cast<array<key_t> &&>(src.key_entries_)),
            val_entries_(static_cast<array<val_t> &&>(src.val_entries_)) {
        }

        sorted_map &operator = (const sorted_map &other) {
            if (this == &other)
                return *this;
            key_entries_ = other.key_entries_;
            val_entries_ = other.val_entries_;
            comparator__ = other.comparator__;
            return *this;
        }

        sorted_map &operator = (sorted_map &&other) noexcept {
            if (this == &other)
                return *this;
            key_entries_ = static_cast<array<key_t> &&>(other.key_entries_);
            val_entries_ = static_cast<array<val_t> &&>(other.val_entries_);
            comparator__ = other.comparator__;
            return *this;
        }

        int binary_search(const key_t &key) const {
            return key_entries_.binary_search(key, comparator__);
        }

        void put(const entry &entry) {
            put(entry.key, entry.val);
        }

        void put(const key_t &key, const val_t &element) {
            const int old_size = key_entries_.size;
            const int index = key_entries_.sort_insert_once(key, comparator__);
            if (old_size < size) val_entries_.insert(index, element);
            else                 val_entries_[index] = element;
        }

        int index_at(const key_t &key) const {
            const int index = binary_search(key);
            if (index < 0 || index >= size)
                abort();
            return index;
        }

        key_t key_at_index(const int index) const {
            return key_entries_[index];
        }

        key_t &key_at_index(const int index) {
            return key_entries_[index];
        }

        val_t &at(const key_t &key) {
            const int index = binary_search(key);
            if (index < 0 || index >= size)
                abort();
            return val_entries_[index];
        }

        const val_t &at(const key_t &key) const {
            const int index = binary_search(key);
            if (index < 0 || index >= size)
                abort();
            return val_entries_[index];
        }

        const val_t &at_index(const int &index) const {
            return val_entries_[index];
        }

        val_t &at_index(const int &index) {
            return val_entries_[index];
        }

        bool contains(const key_t &key) const {
            const int index = binary_search(key);
            if (index < 0 || index >= size)
                return false;
            return true;
        }

        void collapse(const key_t &key) {
            const int index = binary_search(key, comparator__);
            if (index < 0 || index >= size)
                return;
            key_entries_.collapse(index);
            val_entries_.collapse(index);
        }

        void collapse_at_pos(const int &idx) {
            if (idx >= key_entries_.size || idx < 0)
                return;
            key_entries_.collapse(idx);
            val_entries_.collapse(idx);
        }

        bool empty() const {
            return key_entries_.empty();
        }

        void clean() {
            key_entries_.clean();
            val_entries_.clean();
        }

        void reserve(const int n) {
            if (n <= capacity)
                return;
            key_entries_.reserve(n);
            val_entries_.reserve(n);
        }
    };

}

#endif //EX_LIMBO_DATA_SORTED_MAP_H
