//
// Created by henryco on 12/03/25.
//

#ifndef EX_LIMBO_DATA_RB_MAP_H
#define EX_LIMBO_DATA_RB_MAP_H

#include "../alloc/datalloc.h"
#include <cstring>
#include <new>

namespace ex::data {

    /**
     * Red-Black Tree implementation of Map
     */
    template<typename key_t, typename val_t>
    struct rb_map {

        struct entry {
            key_t key;
            val_t val;
        };

        using col_t = bool;
#define BLACK_        false
#define RED_          true

        struct rb_node {
            rb_node *par;
            rb_node *lns;
            rb_node *rns;
            val_t    val;
            key_t    key;
            col_t    col;
        };

#ifdef UNIT_TEST
        using metrics_t = int;
        inline static metrics_t metrics_n_ = 0;
#endif

        using comparator_fn = int32_t (*)(const key_t *a, const key_t *b);
        static int32_t default_comparator(const key_t *a, const key_t *b) {
            return (*a > *b) - (*a < *b);
        }

        allocator allocator_;

        comparator_fn comp_;
        int64_t       size_;
        rb_node      *root_;

        rb_map():
            comp_(&rb_map::default_comparator), size_(0), root_(nullptr) {
        }

        rb_map(const comparator_fn compare):
            comp_(compare), size_(0), root_(nullptr) {
        }

        rb_map(const allocator &allocator_):
            comp_(&rb_map::default_comparator), size_(0), root_(nullptr) {
            this->allocator_ = allocator_;
        }

        rb_map(const allocator &allocator_, const comparator_fn compare):
            comp_(compare), size_(0), root_(nullptr) {
            this->allocator_ = allocator_;
        }

        rb_map(const rb_map &other):
            comp_(other.comp_), size_(other.size_) {
            root_      = other.iterative_copy_(other.root_);
            allocator_ = other.allocator_;
        }

        rb_map(rb_map &&other) noexcept:
            comp_(other.comp_), size_(other.size_), root_(other.root_) {
            allocator_ = other.allocator_;
            other.root_ = nullptr;
            other.size_ = 0;
        }

        rb_map &operator = (const rb_map &other) {
            if (this == &other)
                return *this;
            clean();
            root_      = other.iterative_copy_(other.root_);
            allocator_ = other.allocator_;
            comp_      = other.comp_;
            size_      = other.size_;
            return *this;
        }

        rb_map &operator = (rb_map &&other) noexcept {
            if (this == &other)
                return *this;
            clean();
            allocator_ = other.allocator_;
            comp_      = other.comp_;
            size_      = other.size_;
            root_      = other.root_;
            other.root_ = nullptr;
            other.size_ = 0;
            return *this;
        }

        ~rb_map() {
            iterative_free_(root_);
            root_ = nullptr;
        }

        void clean() {
            iterative_free_(root_);
            root_ = nullptr;
            size_ = 0;
        }

        bool empty() const {
            return size_ <= 0;
        }

        int64_t size() const {
            return size_;
        }

        rb_node *next(const key_t &key) const {
            int direction;
#ifdef UNIT_TEST
            rb_node *node = descend_(root_, key, &direction, &metrics_n_);
#else
            rb_node *node = descend_(root_, key, &direction);
#endif
            if (node == nullptr || direction != 0 || comp_(&key, &(node->key)) != 0)
                abort_("");
            if (node->rns != nullptr) {
                if (rb_node *min_ = minimum_(node->rns))
                    return min_;
                abort_("");
            }
            for (rb_node *head = node;;) {
                rb_node *parent = head->par;
                if (parent == nullptr)
                    return nullptr;
                if (parent->lns == head)
                    return parent;
                head = parent;
            }
        }

        rb_node *prev(const key_t &key) const {
            int direction;
#ifdef UNIT_TEST
            rb_node *node = descend_(root_, key, &direction, &metrics_n_);
#else
            rb_node *node = descend_(root_, key, &direction);
#endif
            if (node == nullptr || direction != 0 || comp_(&key, &(node->key)) != 0)
                abort_("");
            if (node->lns != nullptr) {
                if (rb_node *max_ = maximum_(node->lns))
                    return max_;
                abort_("");
            }
            for (rb_node *head = node;;) {
                rb_node *parent = head->par;
                if (parent == nullptr)
                    return nullptr;
                if (parent->rns == head)
                    return parent;
                head = parent;
            }
        }

        rb_node *last() const {
            if (root_ == nullptr)
                return nullptr;
            return maximum_(root_);
        }

        rb_node *first() const {
            if (root_ == nullptr)
                return nullptr;
            return minimum_(root_);
        }

        bool contains(const key_t &key) const {
            int direction;
#ifdef UNIT_TEST
            rb_node *node = descend_(root_, key, &direction, &metrics_n_);
#else
            rb_node *node = descend_(root_, key, &direction);
#endif
            if (node == nullptr)
                return false;

            if (direction != 0 || comp_(&key, &(node->key)) != 0)
                return false;

            return true;
        }

        val_t &at(const key_t &key) const {
            int direction;

#ifdef UNIT_TEST
            rb_node *node = descend_(root_, key, &direction, &metrics_n_);
#else
            rb_node *node = descend_(root_, key, &direction);
#endif

            if (node == nullptr)
                abort_(""); // all or nothing, avoid exceptions

            if (direction == 0 && comp_(&key, &(node->key)) == 0)
                return node->val;

            abort_(""); // all or nothing, avoid exceptions
        }

        val_t &operator [] (const key_t &key) {

            if (root_ == nullptr) {
                root_      = instance_node_();
                root_->par = nullptr;
                root_->lns = nullptr;
                root_->rns = nullptr;
                root_->val = val_t();
                root_->key = key;
                root_->col = BLACK_;
                ++size_;
                return root_->val;
            }

            int direction;

#ifdef UNIT_TEST
            rb_node *base = descend_(root_, key, &direction, &metrics_n_);
#else
            rb_node *base = descend_(root_, key, &direction);
#endif

            if (direction == 0 && base != nullptr && comp_(&key, &(base->key)) == 0)
                return base->val;

            ++size_;

            rb_node *node = instance_node_();
            node->par     = base;
            node->lns     = nullptr;
            node->rns     = nullptr;
            node->val     = val_t();
            node->key     = key;
            node->col     = RED_;

            if (direction >= 0) base->rns = node;
            else                base->lns = node;

            fix_insert_if_needed_(node);

            root_->col = BLACK_; // just in case
            return node->val;
        }

        const val_t &operator [] (const key_t &key) const {
            return at(key);
        }

        void put(const entry &entry) {
            put(entry.key, entry.val);
        }

        void put(const key_t &key, const val_t &val) {

            if (root_ == nullptr) {
                root_      = instance_node_();
                root_->par = nullptr;
                root_->lns = nullptr;
                root_->rns = nullptr;
                root_->val = val;
                root_->key = key;
                root_->col = BLACK_;
                ++size_;
                return;
            }

            int direction;

#ifdef UNIT_TEST
            rb_node *base = descend_(root_, key, &direction, &metrics_n_);
#else
            rb_node *base = descend_(root_, key, &direction);
#endif

            if (direction == 0 && comp_(&key, &(base->key)) == 0) {
                base->val = val;
                return;
            }

            ++size_;

            rb_node *node = instance_node_();
            if (node == root_)
                abort_("");
            node->par     = base;
            node->lns     = nullptr;
            node->rns     = nullptr;
            node->val     = val;
            node->key     = key;
            node->col     = RED_;

            if (direction >= 0) base->rns = node;
            else                base->lns = node;

            fix_insert_if_needed_(node);

            // if root_ is nullptr we have much bigger problem
            root_->col = BLACK_; // just in case
        }

        void remove(const key_t &key) {
            int direction;

#ifdef UNIT_TEST
            rb_node *node = descend_(root_, key, &direction, &metrics_n_);
#else
            rb_node *node = descend_(root_, key, &direction);
#endif

            if (node == nullptr)
                return;

            if (direction != 0 || comp_(&key, &(node->key)) != 0)
                return;

            remove_node_(node);
        }

        void remove_node_(rb_node *node) {
            if (--size_ == 0) {
                clean();
                return;
            }

            if (node->lns == nullptr && node->rns == nullptr) {
                if (node == root_) {
                    // node is a root, and node has no any children
                    clean();
                    return;
                }

                rb_node *sibling = get_sibling_(node);
                replace_with_child_(node, nullptr);
                fix_delete_if_needed_(node, nullptr, sibling);
                free_node_(node);

                return;
            }

            if (node->lns != nullptr && node->rns != nullptr) {
                // left and right
                rb_node *largest = get_largest_(node->lns);
                rb_node *child   = largest->lns;

                node->key = largest->key;
                node->val = largest->val;

                rb_node *sibling = get_sibling_(largest);
                replace_with_child_(largest, child);
                fix_delete_if_needed_(largest, child, sibling);
                free_node_(largest);

                return;
            }

            if (node->lns != nullptr && node->rns == nullptr) {
                // left
                rb_node *child = node->lns;

                if (node == root_) {
                    root_ = child;
                    root_->par = nullptr;
                    root_->col = BLACK_;
                    free_node_(node);
                    return;
                }

                rb_node *sibling = get_sibling_(node);
                replace_with_child_(node, child);
                fix_delete_if_needed_(node, child, sibling);
                free_node_(node);

                return;
            }

            if (node->rns != nullptr && node->lns == nullptr) {
                // right
                rb_node *child = node->rns;

                if (node == root_) {
                    root_ = child;
                    root_->par = nullptr;
                    root_->col = BLACK_;
                    free_node_(node);
                    return;
                }

                rb_node *sibling = get_sibling_(node);
                replace_with_child_(node, child);
                fix_delete_if_needed_(node, child, sibling);
                free_node_(node);

                return;
            }

            abort_("");
        }

        void fix_delete_if_needed_(rb_node *node, rb_node *child, rb_node *sibling) {
            // case 1: deleted was red
            if (node == nullptr || node->col == RED_)
                return;

            // case 2: deleted was black with a red child
            if (child != nullptr && child->col == RED_) {
                child->col = BLACK_;
                return;
            }

            fix_double_black_(child, sibling, node->par);
            if (root_ != nullptr)
                root_->col = BLACK_; // just in case
        }

        void fix_double_black_(rb_node *current, rb_node *sibling, rb_node *parent) {
            if (parent == nullptr)
                return;

            // case 2: deleted was black with black sibling (NULL is implicitly black)
            if (sibling == nullptr) {
                if (parent == root_)
                    return;

                if (parent->col == RED_) {
                    parent->col = BLACK_;
                    return;
                }

                fix_double_black_(parent, get_sibling_(parent), parent->par);
                return;
            }

            // case 1: sibling is Red
            if (sibling->col == RED_) {
                swap_colors(parent, sibling);

                if (current == parent->lns)
                    rotate_left_(parent);
                else if (current == parent->rns)
                    rotate_right_(parent);

//                parent = current->par; // parent does not change after rotation

                if (current != nullptr) sibling = get_sibling_(current);
                else                    sibling = (parent->lns == current) ? parent->rns : parent->lns;

                if (sibling == nullptr) {
                    // sibling is implicitly black (case 2)
                    fix_double_black_(parent, nullptr, parent->par);
                    return;
                }
            }

            // case 2: sibling is black, both of siblings children are black
            if ((sibling->lns == nullptr || sibling->lns->col == BLACK_) &&
                (sibling->rns == nullptr || sibling->rns->col == BLACK_)) {

                sibling->col = RED_;

                if (parent == root_ || parent->col == RED_) {
                    parent->col = BLACK_;
                    return;
                }

                fix_double_black_(parent, get_sibling_(parent), parent->par);
                return;
            }

            const bool is_left = (parent->lns == current);

            // case 3: sibling is black, far child is red
            if (( is_left && (sibling->rns != nullptr && sibling->rns->col == RED_)) ||
                (!is_left && (sibling->lns != nullptr && sibling->lns->col == RED_))) {

                swap_colors(parent, sibling);

                if (is_left) {
                    rotate_left_(parent);
                    if (parent->par != nullptr && parent->par->rns != nullptr)
                        parent->par->rns->col = BLACK_;
                }
                else {
                    rotate_right_(parent);
                    if (parent->par != nullptr && parent->par->lns != nullptr)
                        parent->par->lns->col = BLACK_;
                }
                return;
            }

            // case 4: sibling is black, near child is red
            if (( is_left && (sibling->lns != nullptr && sibling->lns->col == RED_)) ||
                (!is_left && (sibling->rns != nullptr && sibling->rns->col == RED_))) {

                rb_node *near = is_left ? sibling->lns : sibling->rns;
                if (near == sibling->lns) {
                    sibling->lns->col = BLACK_;
                    sibling->col      = RED_;

                    rotate_right_(sibling);

                    sibling = parent->rns;

                    swap_colors(parent, sibling);
                    rotate_left_(parent);

                    if (sibling->rns != nullptr)
                        sibling->rns->col = BLACK_;
                }
                else {
                    sibling->rns->col = BLACK_;
                    sibling->col      = RED_;

                    rotate_left_(sibling);

                    sibling = parent->lns;

                    swap_colors(parent, sibling);
                    rotate_right_(parent);

                    if (sibling->lns != nullptr)
                        sibling->lns->col = BLACK_;
                }
            }
        }

        void fix_insert_if_needed_(rb_node *node) {
            rb_node *parent = node->par;

            if (parent == nullptr) {
                if (node != root_)
                    abort_("");

                // just in case, root is always black
                node->col = BLACK_;
                return;
            }

            if (parent->col == BLACK_)
                return;

            rb_node *grand_parent = parent->par;
            if (grand_parent == nullptr) {
                // just in case, root is always black
                parent->col = BLACK_;
            }

            rb_node *uncle = get_sibling_(parent);

            // Case 1: Parent and uncle are both red -> Recolor and recurse up
            if (uncle != nullptr && uncle->col == RED_) {
                parent->col       = BLACK_;
                uncle->col        = BLACK_;

                if (grand_parent != nullptr) {
                    grand_parent->col = RED_;
                    fix_insert_if_needed_(grand_parent);
                }
                return;
            }

            // Case 2a: Left-Left (LL) violation
            if (node == parent->lns && parent == grand_parent->lns) {
                swap_colors(parent, grand_parent);
                rotate_right_(grand_parent);
            }

            // Case 2b: Right-Right (RR) violation
            else if (node == parent->rns && parent == grand_parent->rns) {
                swap_colors(parent, grand_parent);
                rotate_left_(grand_parent);
            }

            // Case 3a: Left-Right (LR) violation
            else if (node == parent->lns && parent == grand_parent->rns) {
                swap_colors(node, grand_parent);
                rotate_right_(parent);
                rotate_left_(grand_parent);
            }

            // Case 3b: Right-Left (RL) violation
            else if (node == parent->rns && parent == grand_parent->lns) {
                swap_colors(node, grand_parent);
                rotate_left_(parent);
                rotate_right_(grand_parent);
            }

            if (root_ != nullptr)
                root_->col = BLACK_; // just in case
        }

        rb_node *descend_(rb_node *node, const key_t &key, int *direction, int *n = nullptr) const {
#ifdef UNIT_TEST
            if (n) *n = 0;
#endif
            while (node != nullptr) {
#ifdef UNIT_TEST
                if (n) (*n)++;
#endif

                *direction = comp_(&key, &(node->key));

                if (*direction == 0)
                    return node;

                if (*direction > 0) {
                    if (node->rns != nullptr) node = node->rns;
                    else return node;
                } else {
                    if (node->lns != nullptr) node = node->lns;
                    else return node;
                }
            }
            return nullptr;
        }

        void rotate_right_(rb_node *node) {
            rb_node *left = node->lns;
            if (left == nullptr)
                return;

            node->lns = left->rns;
            if (left->rns != nullptr)
                left->rns->par = node;

            left->par = node->par;

            if (node->par == nullptr) {
                root_ = left;
            }
            else if (node == node->par->lns) {
                node->par->lns = left;
            }
            else {
                node->par->rns = left;
            }

            left->rns = node;
            node->par = left;
        }

        void rotate_left_(rb_node *node) {
            rb_node *right = node->rns;
            if (right == nullptr)
                return;

            node->rns = right->lns;
            if (right->lns != nullptr)
                right->lns->par = node;

            right->par = node->par;

            if (node->par == nullptr) {
                root_ = right;
            }
            else if (node == node->par->lns) {
                node->par->lns = right;
            }
            else {
                node->par->rns = right;
            }

            right->lns = node;
            node->par  = right;
        }

        void replace_with_child_(rb_node *node, rb_node *child) {
            if (node->par == nullptr)
                root_ = child;
            else if (node == node->par->lns)
                node->par->lns = child;
            else
                node->par->rns = child;

            if (child != nullptr)
                child->par = node->par;
        }

        rb_node *instance_node_() {
            return new (malloc_<rb_node>(this, allocator_, 1)) rb_node;
        }

        rb_node *copy_node_(const rb_node *node) const {
            if (node == nullptr)
                return nullptr;
            rb_node *copy = instance_node_();
            *copy = *node;
            return copy;
        }

        void iterative_copy_(rb_node *const node) {
            rb_node *copy = copy_node_(node);
            rb_node *head = node;
            rb_node *prev = node;
            bool     down = true;

            while (head != nullptr) {
                if (down && head->lns != nullptr) {
                    prev = head;
                    head = head->lns;

                    *copy->lns = copy_node_(head);
                    copy = copy->lns;
                    continue;
                }

                if (down && head->rns != nullptr) {
                    prev = head;
                    head = head->rns;

                    *copy->rns = copy_node_(head);
                    copy = copy->rns;
                    continue;
                }

                if (down || prev == head->rns) {
                    prev = head;
                    head = head->par;
                    copy = copy->par;
                    down = false;
                    continue;
                }

                if (head->rns != nullptr) {
                    prev = head;
                    head = head->rns;

                    *copy->rns = copy_node_(head);
                    copy = copy->rns;
                    down = true;
                    continue;
                }

                prev = head;
                head = head->par;
                copy = copy->par;
                down = false;
            }
        }

        void free_node_(rb_node *node) {
            if (node == nullptr)
                return;
            if (node->par != nullptr) {
                if (node->par->lns == node)
                    node->par->lns = nullptr;
                if (node->par->rns == node)
                    node->par->rns = nullptr;
            }
            release_node_(node);
        }

        void release_node_(rb_node *node) {
            if (node == nullptr)
                return;
            node->~rb_node();
            free_(this, allocator_, node);
        }

        void iterative_free_(rb_node *const node) {
            rb_node *head = node;
            rb_node *prev = node;
            bool     down = true;

            while (head != nullptr) {
                if (down && head->lns != nullptr) {
                    prev = head;
                    head = head->lns;
                    continue;
                }

                if (down && head->rns != nullptr) {
                    prev = head;
                    head = head->rns;
                    continue;
                }

                if (down || prev == head->rns) {
                    prev = head;
                    head = head->par;
                    down = false;
                    release_node_(prev);
                    continue;
                }

                if (head->rns != nullptr) {
                    prev = head;
                    head = head->rns;
                    down = true;
                    continue;
                }

                prev = head;
                head = head->par;
                down = false;
                release_node_(prev);
            }
        }

        static rb_node *get_largest_(rb_node *node) {
            if (node == nullptr)
                return nullptr;
            if (node->rns == nullptr)
                return node;
            return get_largest_(node->rns);
        }

        static rb_node *get_sibling_(const rb_node *node) {
            if (node == nullptr)
                abort_("");

            const rb_node *parent = node->par;
            if (parent == nullptr)
                return nullptr;
            return parent->lns == node ? parent->rns : parent->lns;
        }

        static void swap_colors(rb_node *a, rb_node *b) {
            const col_t tmp = a->col;
            a->col = b->col;
            b->col = tmp;
        }

        static rb_node *minimum_(rb_node *const node) {
            rb_node *head = node;
            while (head != nullptr && head->lns != nullptr)
                head = head->lns;
            return head;
        }

        static rb_node *maximum_(rb_node *const node) {
            rb_node *head = node;
            while (head != nullptr && head->rns != nullptr)
                head = head->rns;
            return head;
        }

        // DSF Post-Order iterator
        struct iterator {
//            entry    entr_;
            rb_node *head_;
            rb_node *prev_;
            long     idx__;
            bool     down_;

            iterator(rb_node *node) {
//                entr_ = {};
                head_ = node;
                prev_ = node;
                down_ = true;
                idx__ = -1;
                spin_();
            }

//            entry &operator * () {
//                return entr_;
//            }

            rb_node &operator * () {
                if (prev_ == nullptr)
                    abort_("");
                return *prev_;
            }

            void spin_() {
                while (head_ != nullptr) {
                    if (down_ && head_->lns != nullptr) {
                        prev_ = head_;
                        head_ = head_->lns;
                        continue;
                    }

                    if (down_ && head_->rns != nullptr) {
                        prev_ = head_;
                        head_ = head_->rns;
                        continue;
                    }

                    if (down_ || prev_ == head_->rns) {
                        prev_ = head_;
                        head_ = head_->par;
                        down_ = false;

//                        if (prev_ != nullptr)
//                            entr_ = { prev_->key, prev_->val };
                        idx__++;
                        return;
                    }

                    if (head_->rns != nullptr) {
                        prev_ = head_;
                        head_ = head_->rns;
                        down_ = true;
                        continue;
                    }

                    prev_ = head_;
                    head_ = head_->par;
                    down_ = false;

//                    if (prev_ != nullptr)
//                        entr_ = { prev_->key, prev_->val };
                    idx__++;
                    return;
                }

                idx__ = -1;
            }

            iterator &operator ++ () {
                spin_();
                return *this;
            }

            bool operator != (const iterator &other) const {
                return idx__ != other.idx__;
            }
        };

        iterator begin() const {
            return iterator(root_);
        }

        iterator end() const {
            return iterator(nullptr);
        }

        template <typename T>
        T *malloc_(void *, allocator &allocator, const std::size_t size_n) {
            return static_cast<T *>(allocator.malloc(&allocator, size_n * sizeof(T), sizeof(T)));
        }

        static void free_(void *, allocator &allocator, void *ptr) {
            if (allocator.free != nullptr && ptr != nullptr)
                allocator.free(&allocator, ptr);
        }
    };

}

#endif //EX_LIMBO_DATA_RB_MAP_H
