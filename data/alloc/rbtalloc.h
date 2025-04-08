//
// Created by xd on 3/17/25.
//

#ifndef EX_LIMBO_DATA_RBTALLOC_H
#define EX_LIMBO_DATA_RBTALLOC_H

#include "../struct/rmq_map.h"
#include "stackarena.h"

namespace ex::data::rbtalloc {

// -----------------------------
#define ALLOC_DEF_SIZE_B (1 MB)
#define STACK_ARENA_SIZE (1 MB)
// -----------------------------

    struct memory_block {
        u_int32_t size_b;
        u_int16_t alignment_b;
        u_int8_t  is_free;
    };

    using block_addr_t = void *;
    using block_free_t = u_int32_t;
    using block_map_t  = rmq_map<block_addr_t, memory_block, block_free_t>;

#define BLOCK_MAX_SIZE_B (4294967295U)
#define STACK_BLOCK_SIZE (sizeof(block_map_t::rmq_node))

    struct allocator {

        static int32_t ptr_compare(const block_addr_t *a, const block_addr_t *b) {
            const u_int64_t aa = reinterpret_cast<u_int64_t>(*a);
            const u_int64_t bb = reinterpret_cast<u_int64_t>(*b);
            return (aa > bb) - (aa < bb);
        }

        ex::data::stackarena::allocator arena_ = {
            STACK_BLOCK_SIZE,
            STACK_ARENA_SIZE
        };

        block_map_t  blocks_ = { arena_.compat(), &ptr_compare };
        mem_provider memory_;

        std::size_t used_mem_ = 0;
        std::size_t def_size_ = 0;
        std::size_t grow_mem_ = 0;

        ex::data::allocator compat() {
            return alloc_compat<allocator>::allocator(this);
        }

        allocator(const std::size_t pre_allocated = ALLOC_DEF_SIZE_B) {
            preallocate(pre_allocated);
        }

        allocator(const allocator &other) = delete;
        allocator(allocator &&other)      = delete;

        allocator &operator = (const allocator &other) = delete;
        allocator &operator = (allocator &&other)      = delete;

        ~allocator() {
            blocks_.clean();
            memory_.clean();
            arena_.clean();
        }

        void clean() {
            blocks_.clean();
            memory_.clean();
            arena_.clean();
            used_mem_ = 0;
        }

        void preallocate(const std::size_t pre_allocated = ALLOC_DEF_SIZE_B) {
            if (pre_allocated <= 0)
                abort_("Allocation size must be greater than 0");

            std::size_t        allocated_size = 0;
            const block_addr_t allocated_ptr  = memory_.request(pre_allocated, &allocated_size);

            const auto size_b = static_cast<block_free_t>(allocated_size);
            blocks_.put(allocated_ptr, { size_b, 1, true }, size_b);
            def_size_ = pre_allocated;
            grow_mem_ = pre_allocated;

//            verify_rb_tree_(blocks_);
        }

        block_addr_t malloc(const std::size_t size_b, const std::size_t alignment_b) {
            if (size_b == 0)
                abort_("Empty memory block allocation");
            if (const block_addr_t allocated = block_acquire_(size_b, alignment_b))
                return allocated;
            block_request_(size_b + alignment_b);
            if (const block_addr_t allocated = block_acquire_(size_b, alignment_b))
                return allocated;
            abort_("Out of free space");
        }

        block_addr_t realloc(void *ptr, const std::size_t size_new_b) {
            if (size_new_b == 0 || ptr == nullptr) {
                free(ptr);
                return nullptr;
            }

            const memory_block block = blocks_.at(ptr);
            if (block.is_free == true)
                abort_("Reallocation of free block");

            if (size_new_b == block.size_b)
                return ptr;

            if (size_new_b < block.size_b) {
                const std::size_t  size_d  = block.size_b - size_new_b;
                const memory_block block_l = { static_cast<u_int32_t>(size_new_b), block.alignment_b, 0 };
                const memory_block block_r = { static_cast<u_int32_t>(size_d), 1, 0 };
                const block_addr_t ptr_r   = static_cast<unsigned char *>(ptr) + size_new_b;
                blocks_.put(ptr, block_l, 0);
                blocks_.put(ptr_r, block_r, 0);
                block_free_(ptr_r, block_r);

//                verify_rb_tree_(blocks_);
                return ptr;
            }

            if (block_map_t::rmq_node *next = blocks_.next(ptr);
                continuous_(ptr, next, block.size_b)) {
                const std::size_t  size_d  = size_new_b - block.size_b;
                const memory_block block_n = next->val;
                const memory_block block_l = { static_cast<u_int32_t>(size_new_b), block.alignment_b, 0 };
                const memory_block block_r = { static_cast<u_int32_t>(block_n.size_b - size_d), 1, 1 };
                const block_addr_t ptr_r   = static_cast<unsigned char *>(ptr) + size_new_b;
                const block_addr_t ptr_n   = next->key;
                blocks_.remove(ptr_n);
                blocks_.put(ptr, block_l, 0);
                blocks_.put(ptr_r, block_r, block_r.size_b);
                used_mem_ -= size_d;

//                verify_rb_tree_(blocks_);
                return ptr;
            }

            const block_addr_t allocated = malloc(size_new_b, block.alignment_b);
            if (allocated != ptr) {
                ::memmove(allocated, ptr, size_new_b < block.size_b ? size_new_b : block.size_b);
                block_free_(ptr, block);

//                verify_rb_tree_(blocks_);
                return allocated;
            }

            if (allocated == nullptr)
                abort_("Reallocation address nullptr");
            abort_("Reallocation address invalid");
        }

        void free(const block_addr_t ptr) {
            if (ptr == nullptr)
                return;
            const memory_block block = blocks_.at(ptr);
            block_free_(ptr, block);
        }

        long size_used() const {
            return used_mem_ + arena_.size_used();
        }

        long size_total() const {
            return memory_.size() + arena_.size_total();
        }

        long blocks_total() const {
            return blocks_.size();
        }

        long blocks_free() const {
            u_int32_t c = 0;
            for (auto &block : blocks_) {
                c += (block.val.is_free == true);
            }
            return c;
        }

        // =========================================== INTERNAL UTILS ==================================================

        /**
         * Allocate at least additional <b>min_size_b</b> bytes of memory
         */
        void block_request_(const std::size_t min_size_b) {
//            std::cout << "request block: " << min_size_b << std::endl;

            if (min_size_b > BLOCK_MAX_SIZE_B)
                abort_("Requested allocation size overflow");

            block_map_t::rmq_node *last = blocks_.last();
            if (last == nullptr) {
                preallocate(def_size_);
                last = blocks_.last();
                if (last == nullptr)
                    abort_("Allocator not initialized");
            }

            const block_addr_t last_ptr   = last->key;
            const memory_block last_block = last->val;

            grow_mem_ = grow_mem_ + grow_mem_ + min_size_b;

            std::size_t allocated_size = 0;
            void *allocated_ptr = memory_.request(grow_mem_, &allocated_size);

            if (continuous_(last_ptr, allocated_ptr, last_block.size_b)) {
                // extend last free block which belongs to the same memory region
                if (last_block.is_free == true) {
                    last->val.size_b += allocated_size;
                    last->rmq = last->val.size_b;
                    blocks_.update_ranges_(last);
                    return;
                }
            }

            // append new free block
            const auto size_b = static_cast<block_free_t>(allocated_size);
            blocks_.put(allocated_ptr, { size_b, 1, true }, size_b);
        }

        block_addr_t block_acquire_(const std::size_t size_b, const std::size_t alignment_b) {
            if (size_b == 0)
                abort_("Requested blocks size must be non-zero");
            if ((size_b + alignment_b) > BLOCK_MAX_SIZE_B)
                abort_("Requested allocation size overflow");
            if (alignment_b == 0 || (alignment_b & (alignment_b - 1)) != 0)
                abort_("Alignment must be non-zero and a power of two");

            memory_block test_that           = { static_cast<u_int32_t>(size_b), static_cast<u_int16_t>(alignment_b) };
            const block_map_t::rmq_node *fit = blocks_.range_fit(size_b, BLOCK_MAX_SIZE_B, &test_padding_, &test_that);
            if (fit == nullptr)
                return nullptr;

            const memory_block fit_block = fit->val;
            void              *fit_key   = fit->key;
            const u_int32_t    padding   = block_padding_(fit_key, alignment_b);

            const u_int32_t size_ls = static_cast<u_int32_t>(size_b);
            const u_int32_t size_rs = fit_block.size_b - size_ls - padding;

            const block_addr_t l_ptr   = static_cast<unsigned char *>(fit_key) + padding;
            const memory_block l_block = { size_ls, static_cast<u_int16_t>(alignment_b), 0 };
            blocks_.put(l_ptr, l_block, 0);

//            verify_rb_tree_(blocks_);

            if (padding > 0) {
                const memory_block f_block = { padding, 1, 1 };
                blocks_.put(fit_key, f_block, padding);

//                verify_rb_tree_(blocks_);
            }

            if (size_rs > 0) {
                const block_addr_t r_ptr   = static_cast<unsigned char *>(l_ptr) + size_ls;
                const memory_block r_block = { size_rs, 1, 1 };
                blocks_.put(r_ptr, r_block, size_rs);

//                verify_rb_tree_(blocks_);
            }

            used_mem_ += size_ls;
            return l_ptr;
        }

        void block_free_(const block_addr_t ptr, const memory_block &block) {
            if (block.is_free == true)
                abort_("Memory block double free");

            used_mem_ -= block.size_b;

            memory_block free_block = { block.size_b, 1, 1 };
            block_addr_t free_ptr   = ptr;

            const block_map_t::rmq_node *next = blocks_.next(ptr);
            const block_map_t::rmq_node *prev = blocks_.prev(ptr);

            block_addr_t prev_key = nullptr;
            block_addr_t next_key = nullptr;

            if (next != nullptr && next->val.is_free == true) {
                if (continuous_(ptr, next->key, block.size_b)) {
                    // merging next block which belongs to the same memory region
                    free_block.size_b += next->val.size_b;
                    next_key = next->key;
                }
            }

            if (prev != nullptr && prev->val.is_free == true) {
                if (continuous_(prev->key, ptr, prev->val.size_b)) {
                    // merging prev block which belongs to the same memory region
                    free_block.size_b += prev->val.size_b;
                    prev_key = prev->key;
                    free_ptr = prev->key;
                }
            }

            if (next_key != nullptr) {
                safe_remove_(next_key);
            }

            if (prev_key != nullptr) {
                safe_remove_(prev_key);
            }

            safe_remove_(ptr);

            blocks_.put(free_ptr, free_block, free_block.size_b);
//            verify_rb_tree_(blocks_);
        }

        void safe_remove_(void *ptr) {
//            block_map_t::rmq_node *replacement =
                blocks_.remove(ptr);
//            verify_rb_tree_(blocks_);
//            if (replacement != nullptr) {
//                const memory_block block = replacement->val;
//                replacement->rmq = block.is_free == true ? replacement->val.size_b : 0;
//                blocks_.update_ranges_(replacement);
//            }
        }

        static bool continuous_(void *prev, void *next, const u_int64_t prev_size) {
            if (next == nullptr || prev == nullptr)
                return false;
            return (static_cast<unsigned char *>(prev) + prev_size) == next;
        }

        static bool test_padding_(const block_map_t::rmq_node *node, void *this_) {
            const memory_block *that_   = static_cast<memory_block *>(this_);
            const u_int64_t     padding = block_padding_(node->key, that_->alignment_b);
            return node->val.size_b >= (that_->size_b + padding);
        }

        static u_int32_t block_padding_(const void *ptr, const u_int64_t alignment_b) {
            const u_int64_t  address  = reinterpret_cast<u_int64_t>(ptr);
            return (alignment_b - (address % alignment_b)) % alignment_b;
        }

//        static void verify_rb_tree_(block_map_t &map) {
//            u_int32_t min = 0;
//            u_int32_t max = 0;
//            if (!verify_rmq_weights_(map.root_, min, max))
//                abort_("tree structure invalid");
//        }
//
//        static bool verify_rmq_weights_(block_map_t::rmq_node *node, u_int32_t &min, u_int32_t &max) {
//            if (node == nullptr) return true;
//
//            u_int32_t min_l = node->rmq, max_l = node->rmq;
//            u_int32_t min_r = node->rmq, max_r = node->rmq;
//
//            if (node->lns != nullptr && !verify_rmq_weights_(node->lns, min_l, max_l))
//                return false;
//
//            if (node->rns != nullptr && !verify_rmq_weights_(node->rns, min_r, max_r))
//                return false;
//
//            min = std::min(node->rmq, std::min(min_l, min_r));
//            max = std::max(node->rmq, std::max(max_l, max_r));
//
//            if ((node->min) != min)
//                abort_("(node->min) != min");
//
//            if ((node->max) != max)
//                abort_("(node->max) != max");
//
//            if (node->val.is_free == false && node->rmq != 0)
//                abort_("node->val.is_free == false && node->rmq != 0");
//
//            if (node->val.is_free == true && node->rmq == 0)
//                abort_("node->val.is_free == true && node->rmq == 0");
//
//            if (node->val.is_free == true && node->val.size_b != node->rmq)
//                abort_("node->val.is_free == true && node->val.size_b != node->rmq");
//
//            return node->min == min && node->max == max;
//        }
    };

}

#endif //EX_LIMBO_DATA_RBTALLOC_H
