//
// Created by xd on 3/24/25.
//

#ifndef EX_LIMBO_DATA_LIST_MAP_H
#define EX_LIMBO_DATA_LIST_MAP_H

#include "../alloc/datalloc.h"
#include <new>

namespace ex::data {

    /**
     * Linked-list backed implementation of Map
     */
    template<typename key_t, typename val_t>
    struct list_map {

        struct entry {
            key_t key;
            val_t val;
        };

        struct list_node {
            list_node *prev;
            list_node *next;
            val_t      val;
            key_t      key;
        };

        using sort_compare_fn = int8_t (*)(const list_node *a, const list_node *b);
        using comparator_fn   = int8_t (*)(const key_t *a, const key_t *b);
        static int8_t default_comparator(const key_t *a, const key_t *b) {
            return (*a > *b) - (*a < *b);
        }

        allocator     allocator_;
        comparator_fn comp_;
        int64_t       size_;
        list_node    *root_;
        list_node    *back_;

        list_map():
            comp_(&list_map::default_comparator), size_(0), root_(nullptr), back_(nullptr) {
        }

        list_map(const comparator_fn compare):
            comp_(compare), size_(0), root_(nullptr), back_(nullptr) {
        }

        list_map(const allocator &allocator_):
            allocator_(allocator_), comp_(&list_map::default_comparator),
            size_(0), root_(nullptr), back_(nullptr) {
        }

        list_map(const allocator &allocator_, const comparator_fn compare) :
            allocator_(allocator_), comp_(compare),
            size_(0), root_(nullptr), back_(nullptr) {
        }

        list_map(const list_map &other) {
            allocator_ = other.allocator_;
            comp_      = other.comp_;
            full_copy_(other.root_, &size_, &root_, &back_);
        }

        list_map(list_map &&other) noexcept {
            allocator_  = other.allocator_;
            comp_       = other.comp_;
            root_       = other.root_;
            back_       = other.back_;
            size_       = other.size_;
            other.root_ = nullptr;
            other.back_ = nullptr;
            other.size_ = 0;
        }

        list_map &operator = (const list_map &other) {
            if (this == &other)
                return *this;

            clean();

            allocator_ = other.allocator_;
            comp_      = other.comp_;
            full_copy_(other.root_, &size_, &root_, &back_);

            return *this;
        }

        list_map &operator = (list_map &&other) noexcept {
            if (this == &other)
                return *this;

            clean();

            allocator_  = other.allocator_;
            comp_       = other.comp_;
            root_       = other.root_;
            back_       = other.back_;
            size_       = other.size_;
            other.root_ = nullptr;
            other.back_ = nullptr;
            other.size_ = 0;

            return *this;
        }

        ~list_map() {
            for (list_node *head = root_; head != nullptr;) {
                list_node *next = head->next;
                release_node_(head);
                head = next;
            }
        }

        void clean() {
            for (list_node *head = root_; head != nullptr;) {
                list_node *next = head->next;
                release_node_(head);
                head = next;
            }
            root_ = nullptr;
            back_ = nullptr;
            size_ = 0;
        }

        bool empty() const {
            return size_ == 0;
        }

        long size() const {
            return size_;
        }

        list_node *put(const entry &entry) {
            return put(entry.key, entry.val);
        }

        list_node *put(const key_t &key, const val_t &val = val_t()) {
            if (root_ == nullptr) {
                root_       = instance_node_();
                root_->next = nullptr;
                root_->prev = nullptr;
                root_->key  = key;
                root_->val  = val;
                back_       = root_;
                size_       = 1;
                return root_;
            }

            ++size_;

            list_node *node = instance_node_();
            if (node == root_)
                abort_("memory violation");

            back_->next = node;
            node->prev  = back_;
            node->next  = nullptr;
            node->key   = key;
            node->val   = val;

            back_ = node;

            return node;
        }

        list_node *put_sorted(const key_t &key, const val_t &val = val_t()) {
            if (root_ == nullptr) {
                root_       = instance_node_();
                root_->next = nullptr;
                root_->prev = nullptr;
                root_->key  = key;
                root_->val  = val;
                back_       = root_;
                size_       = 1;
                return root_;
            }

            if (comp_(&key, &(root_->key)) < 0) {
                return put_before(root_, key, val);
            }

            if (comp_(&key, &(back_->key)) >= 0) {
                return put_after(back_, key, val);
            }

            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (comp_(&key, &(head->key)) < 0)
                    return put_before(head, key, val);
            }

            abort_("Impossible error - unreachable code area");
        }

        bool contains(const key_t &key) const {
            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (head->key == key)
                    return true;
            }
            return false;
        }

        val_t &operator [] (const key_t &key) {
            if (root_ == nullptr) {
                root_       = instance_node_();
                root_->next = nullptr;
                root_->prev = nullptr;
                root_->key  = key;
                root_->val  = val_t();
                back_       = root_;
                size_       = 1;
                return root_->val;
            }

            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (head->key == key)
                    return head->val;
            }

            list_node *node = instance_node_();
            if (node == root_)
                abort_("");

            ++size_;

            back_->next = node;
            node->prev  = back_;
            node->next  = nullptr;
            node->val   = val_t();
            node->key   = key;

            back_ = node;

            return node->val;
        }

        const val_t &operator [] (const key_t &key) const {
            return at(key);
        }

        val_t &at(const key_t &key) const {
            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (head->key == key)
                    return head->val;
            }
            abort_("Element is not present in map: " << key);
        }

        list_node *get(const key_t &key) const {
            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (head->key == key)
                    return head;
            }
            return nullptr;
        }

        list_node *front() const {
            return root_;
        }

        list_node *back() const {
            return back_;
        }

        void remove_head() {
            remove_node(root_);
        }

        void remove_back() {
            remove_node(back_);
        }

        void remove_node(const key_t &key) {
            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (head->key == key) {
                    remove_node(head);
                    return;
                }
            }
        }

        void remove_node(list_node *node) {
            if (node == nullptr)
                return;
            detach_node(node);
            release_node_(node);
        }

        void detach_node(list_node *node) {
            list_node *prev = node->prev;
            list_node *next = node->next;

            if (prev != nullptr)
                prev->next = next;
            else
                root_ = next;

            if (next != nullptr)
                next->prev = prev;
            else
                back_ = prev;

            --size_;
        }

        void move_front(list_node *node) {
            if (node == nullptr)
                return;
            if (root_ == nullptr) {
                put_front(node->key, node->val);
                return;
            }

            list_node *prev = node->prev;
            list_node *next = node->next;

            if (prev != nullptr)
                prev->next = next;
            else
                root_ = next;

            if (next != nullptr)
                next->prev = prev;
            else
                back_ = prev;

            root_->prev = node;
            node->next  = root_;
            root_       = node;
        }

        void move_back(list_node *node) {
            if (node == nullptr)
                return;

            if (root_ == nullptr) {
                put_front(node->key, node->val);
                return;
            }

            list_node *prev = node->prev;
            list_node *next = node->next;

            if (prev != nullptr)
                prev->next = next;
            else
                root_ = next;

            if (next != nullptr)
                next->prev = prev;
            else
                back_ = prev;

            back_->next = node;
            node->prev  = back_;
            back_       = node;
        }

        list_node *put_front(const key_t &key, const val_t &val = val_t()) {
            if (root_ == nullptr)
                return put(key, val);
            return put_before(root_, key, val);
        }

        list_node *put_back(const key_t &key, const val_t &val = val_t()) {
            if (back_ == nullptr)
                return put(key, val);
            return put_after(back_, key, val);
        }

        list_node *put_after(const key_t &after, const key_t &key, const val_t &val = val_t()) {
            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (head->key == after)
                    return put_after(head, key, val);
            }
            return nullptr;
        }

        list_node *put_before(const key_t &before, const key_t &key, const val_t &val = val_t()) {
            for (list_node *head = root_; head != nullptr; head = head->next) {
                if (head->key == before)
                    return put_before(head, key, val);
            }
            return nullptr;
        }

        list_node *put_after(list_node *node, const key_t &key, const val_t &val = val_t()) {
            if (node == nullptr)
                return nullptr;
            ++size_;

            list_node *new_node = instance_node_();
            if (new_node == root_)
                abort_("");

            list_node *next = node->next;
            node->next      = new_node;

            if (next != nullptr)
                next->prev = new_node;

            new_node->prev  = node;
            new_node->next  = next;
            new_node->key   = key;
            new_node->val   = val;

            if (new_node->prev == back_)
                back_ = new_node;

            return new_node;
        }

        list_node *put_before(list_node *node, const key_t &key, const val_t &val = val_t()) {
            if (node == nullptr)
                return nullptr;
            ++size_;

            list_node *new_node = instance_node_();
            if (new_node == root_)
                abort_("");

            list_node *prev = node->prev;
            node->prev      = new_node;

            if (prev != nullptr)
                prev->next = new_node;

            new_node->prev  = prev;
            new_node->next  = node;
            new_node->key   = key;
            new_node->val   = val;

            if (new_node->next == root_)
                root_ = new_node;

            return new_node;
        }

        void sort() {
            sort_([f = comp_] (const list_node *a, const list_node *b) -> int8_t {
                return f(&(a->key), &(b->key));
            });
        }

        void sort(const sort_compare_fn compare) {
            sort_(compare);
        }

        template<typename F>
        void sort_(const F fn) {
            root_ = merge_sort_(fn, root_);
            if (root_ == nullptr)
                return;
            root_->prev = nullptr;
            for (list_node *head = root_; head != nullptr; head = head->next) {
                back_ = head;
            }
        }

        // ---------------------------------------- GPT GENERATED BULLSHIT ---------------------------------------------
        // Iterative version of my (recurrent) merge sort
        template<typename F>
        static list_node* merge_(const F fn, list_node* a, list_node* b) {
            list_node dummy;
            list_node* tail = &dummy;
            dummy.next = nullptr;

            while (a && b) {
                if (fn(a, b) <= 0) {
                    tail->next = a;
                    a->prev = tail;
                    a = a->next;
                } else {
                    tail->next = b;
                    b->prev = tail;
                    b = b->next;
                }
                tail = tail->next;
            }

            if (a) {
                tail->next = a;
                a->prev = tail;
            } else if (b) {
                tail->next = b;
                b->prev = tail;
            }

            // Fix head node prev pointer
            if (dummy.next) dummy.next->prev = nullptr;
            return dummy.next;
        }

        template<typename F>
        static list_node* merge_sort_(const F fn, list_node* head) {
            if (!head || !head->next)
                return head;

            // Calculate list length
            int list_size = 0;
            list_node* tmp = head;
            while (tmp) {
                list_size++;
                tmp = tmp->next;
            }

            list_node dummy;
            dummy.next = head;
            head->prev = &dummy;

            for (int size = 1; size < list_size; size *= 2) {
                list_node* prev = &dummy;
                list_node* curr = dummy.next;

                while (curr) {
                    // Left sublist
                    list_node* left = curr;
                    int left_size = size;
                    while (left_size-- && curr) curr = curr->next;

                    // Right sublist
                    list_node* right = curr;
                    int right_size = size;
                    while (right_size-- && curr) curr = curr->next;

                    // Save next start point
                    list_node* next = curr;

                    // Disconnect sublists
                    if (left) detach_after(left, size);
                    if (right) detach_after(right, size);

                    // Merge left and right
                    list_node* merged = merge_(fn, left, right);
                    prev->next = merged;
                    if (merged) merged->prev = prev;

                    // Advance prev to the end of merged
                    while (prev->next) prev = prev->next;
                    prev->next = next;
                    if (next) next->prev = prev;

                    curr = next;
                }
            }

            return dummy.next;
        }

        // Helper to disconnect after `count` nodes
        static void detach_after(list_node* start, int count) {
            while (--count && start)
                start = start->next;

            if (start && start->next) {
                start = start->next;
                start->prev->next = nullptr;
                start->prev = nullptr;
            }
        }
        // -------------------------------------------------------------------------------------------------------------



        // ---------------------------------- my own RECURRENT version of merge sort -----------------------------------
//        template<typename F>
//        static list_node *merge_sort_(const F fn, list_node* head) {
//            // Base case: if the list is empty or contains a single element.
//            if (head == nullptr || head->next == nullptr)
//                return head;
//
//            // Split the list into two halves.
//            list_node *second = split_(head);
//
//            // Recursively sort each half.
//            head   = merge_sort_(fn, head);
//            second = merge_sort_(fn, second);
//
//            // Merge the two sorted halves.
//            return merge_(fn, head, second);
//        }
//
//        template<typename F>
//        static list_node *merge_(const F fn, list_node *first, list_node *second) {
//            if (first == nullptr)
//                return second;
//
//            if (second == nullptr)
//                return first;
//
//            // Compare keys to decide the new head.
//            if (fn(first, second) <= 0) {
//                first->next = merge_(fn, first->next, second);
//                if (first->next != nullptr)
//                    first->next->prev = first;
//                first->prev = nullptr;
//                return first;
//            }
//
//            second->next = merge_(fn, first, second->next);
//            if (second->next != nullptr)
//                second->next->prev = second;
//            second->prev = nullptr;
//            return second;
//        }
//
//        static list_node *split_(list_node* head) {
//            list_node *slow = head, *fast = head;
//            while (fast->next != nullptr && fast->next->next != nullptr) {
//                slow = slow->next;
//                fast = fast->next->next;
//            }
//
//            list_node *second = slow->next;
//            slow->next = nullptr;  // Terminate first half.
//            if (second != nullptr)
//                second->prev = nullptr;
//            return second;
//        }
        // -------------------------------------------------------------------------------------------------------------

        list_node *instance_node_() {
            return new (malloc_<list_node>(this, allocator_, 1)) list_node;
        }

        void release_node_(list_node *node) {
            if (node == nullptr)
                return;
            node->~list_node();
            free_(this, allocator_, node);
        }

        list_node *copy_node_(const list_node *node) {
            if (node == nullptr)
                return nullptr;
            list_node *copy = instance_node_();
            copy->prev      = node->prev;
            copy->next      = node->next;
            copy->key       = node->key;
            copy->val       = node->val;
            return copy;
        }

        void full_copy_(const list_node *node, int64_t *size, list_node **root, list_node **back) {
            if (node == nullptr) {
                *size = 0;
                *root = nullptr;
                *back = nullptr;
                return;
            }

            *size = 1;
            *root = copy_node_(node);
            for (list_node *head = *root; head != nullptr; head = head->next) {
                if (list_node *next = head->next; next != nullptr) {
                    list_node *copy = copy_node_(next);
                    copy->prev      = head;
                    head->next      = copy;
                    *back           = copy;
                    (*size)++;
                }
            }
        }

        struct iterator {
            list_node *head_;

            iterator(list_node *node) :
                head_(node) {
            }

            list_node &operator * () const {
                return *head_;
            }

            iterator &operator ++ () {
                head_ = head_->next;
                return *this;
            }

            iterator &operator -- () {
                head_ = head_->prev;
                return *this;
            }

            iterator &operator ++ (int) {
                head_ = head_->next;
                return *this;
            }

            iterator &operator -- (int) {
                head_ = head_->prev;
                return *this;
            }

            bool operator != (const iterator &other) const {
                return head_ != other.head_;
            }

            bool operator == (const iterator &other) const {
                return head_ == other.head_;
            }
        };

        iterator begin() const {
            return iterator(root_);
        }

        iterator end() const {
            return iterator(back_);
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

    template<typename T>
    using linked_list = list_map<T, void *>;

}

#endif //EX_LIMBO_DATA_LIST_MAP_H
