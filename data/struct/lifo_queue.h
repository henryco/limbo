//
// Created by henryco on 30/03/25.
//

#ifndef EX_LIMBO_DATA_LIFO_QUEUE_H
#define EX_LIMBO_DATA_LIFO_QUEUE_H

#include "../alloc/datalloc.h"
#include <new>

namespace ex::data {

    template<typename val_t>
    struct lifo_queue {

        struct lifo_node {
            lifo_node *next;
            lifo_node *prev;
            val_t       val;
        };

        allocator  allocator_;
        lifo_node *head_;
        lifo_node *root_;
        int64_t    size_;

        lifo_queue():
            head_(nullptr),
            root_(nullptr),
            size_(0) {
        }

        lifo_queue(const allocator &allocator_):
            allocator_(allocator_),
            head_(nullptr),
            root_(nullptr),
            size_(0) {
        }

        lifo_queue(const lifo_queue &other) = delete;
        lifo_queue(lifo_queue &&other)      = delete;

        lifo_queue &operator = (const lifo_queue &other) = delete;
        lifo_queue &operator = (lifo_queue &&other)      = delete;

        ~lifo_queue() {
            lifo_node *node = head_;
            while (node != nullptr) {
                lifo_node *prev = node->prev;
                release_node_(node);
                node = prev;
            }
        }

        void clear() {
            lifo_node *node = head_;
            while (node != nullptr) {
                lifo_node *prev = node->prev;
                release_node_(node);
                node = prev;
            }
            head_ = nullptr;
            root_ = nullptr;
            size_ = 0;
        }

        lifo_node *root() {
            return root_;
        }

        lifo_node *peek() {
            return head_;
        }

        val_t pop() {
            val_t value = head_->val;
            remove(head_);
            return value;
        }

        val_t root_remove() {
            val_t value = root_->val;
            remove(root_);
            return value;
        }

        lifo_node *push(const val_t &val = val_t()) {
            return push(instance_node_(val));
        }

        lifo_node *push(lifo_node *node) {
            if (node == nullptr)
                return nullptr;

            size_++;

            if (root_ == nullptr) {
                size_ = 1;
                head_ = node;
                root_ = node;
                head_->prev = nullptr;
                head_->next = nullptr;
                return head_;
            }

            node->next  = nullptr;
            node->prev  = head_;

            head_->next = node;
            head_       = node;
            return head_;
        }

        void remove() {
            remove(head_);
        }

        bool contains(lifo_node *node) {
            for (lifo_node *head = head_; head != nullptr; head = head->prev) {
                if (head == node)
                    return true;
            }
            return false;
        }

        void remove(lifo_node *node) {
            if (node == nullptr || head_ == nullptr || root_ == nullptr) {
                size_ = 0;
                return;
            }

            lifo_node *prev = node->prev;
            lifo_node *next = node->next;

            if (prev != nullptr)
                prev->next = next;
            else {
                root_ = next;
                if (root_ != nullptr)
                    root_->prev = nullptr;
            }

            if (next != nullptr)
                next->prev = prev;
            else {
                head_ = prev;
                if (head_ != nullptr)
                    head_->next = nullptr;
            }

            release_node_(node);
            --size_;
        }

        bool empty() const {
            return size_ == 0;
        }

        long size() const {
            return size_;
        }

        struct iterator {
            lifo_node *node_;

            iterator(lifo_node *node) :
                node_(node) {
            }

            lifo_node &operator * () const {
                return *node_;
            }

            iterator &operator ++ () {
                node_ = node_->prev;
                return *this;
            }

            iterator &operator -- () {
                node_ = node_->next;
                return *this;
            }

            iterator operator ++ (int) {
                iterator temp = *this;
                node_ = node_->prev;
                return temp;
            }

            iterator operator -- (int) {
                iterator temp = *this;
                node_ = node_->next;
                return temp;
            }

            bool operator != (const iterator &other) const {
                return node_ != other.node_;
            }

            bool operator == (const iterator &other) const {
                return node_ == other.node_;
            }
        };

        iterator begin() const {
            return iterator(head_);
        }

        iterator end() const {
            return iterator(nullptr);
        }

        lifo_node *instance_node_(const val_t &val) {
            lifo_node *node = new (malloc_<lifo_node>(this, allocator_, 1)) lifo_node;
            node->next      = nullptr;
            node->prev      = nullptr;
            node->val       = val;
            return node;
        }

        void release_node_(lifo_node *node) {
            if (node == nullptr)
                return;
            node->~lifo_node();
            free_(this, allocator_, node);
        }

        using sort_compare_fn = int8_t (*)(const lifo_node *a, const lifo_node *b);

        void sort(const sort_compare_fn compare) {
            sort_(compare);
        }

        template<typename F>
        void sort_(const F fn) {
            root_ = merge_sort_(fn, root_);
            if (root_ == nullptr)
                return;
            root_->prev = nullptr;
            for (lifo_node *head = root_; head != nullptr; head = head->next) {
                head_ = head;
            }
        }

        template <typename T>
        T *malloc_(void *, allocator &allocator, const std::size_t size_n) {
            return static_cast<T *>(allocator.malloc(&allocator, size_n * sizeof(T), alignof(T)));
        }

        static void free_(void *, allocator &allocator, void *ptr) {
            if (allocator.free != nullptr && ptr != nullptr)
                allocator.free(&allocator, ptr);
        }
    };


}

#endif //EX_LIMBO_DATA_LIFO_QUEUE_H
