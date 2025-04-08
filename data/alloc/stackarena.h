//
// Created by xd on 3/16/25.
//

#ifndef EX_LIMBO_DATA_STACK_ARENA_H
#define EX_LIMBO_DATA_STACK_ARENA_H

#include "provider.h"
#include "datalloc.h"
#include <cstring>
#include <cstdint>

namespace ex::data::stackarena {

    template <std::size_t block_s, std::size_t arena_s, std::size_t stack_s = arena_s / block_s * sizeof(void *)>
    struct static_allocator {

        static constexpr std::size_t data_s = arena_s / block_s * block_s;
        static constexpr std::size_t free_s = stack_s / sizeof(void *);

        unsigned char arena_[data_s];
        void         *stack_[free_s];

        std::size_t offset;
        std::size_t free_n;

        ex::data::allocator compat() {
            return alloc_compat<static_allocator>::allocator(this);
        }

        // ReSharper disable once CppPossiblyUninitializedMember
        static_allocator() : offset(0), free_n(0) { // NOLINT(*-pro-type-member-init)
        }

        static_allocator(const static_allocator &other) = delete;
        static_allocator(static_allocator &&other)      = delete;

        static_allocator &operator = (const static_allocator &other) = delete;
        static_allocator &operator = (static_allocator &&other)      = delete;

        /**
         * allocates only one block at a time
         */
        void *malloc(std::size_t, std::size_t) {
            if (free_n > 0)
                return stack_[--free_n];
            if (offset >= data_s)
                abort_("out of memory: " << offset << "b");
            void *ptr = static_cast<void *>(arena_ + offset);
            offset += block_s;
            return ptr;
        }

        void *realloc(void *, const std::size_t) {
            abort_("Realloc not supported");
        }

        /**
         * releases block
         */
        void free(void *ptr) {
            const unsigned char *have = static_cast<unsigned char *>(ptr) + block_s;
            const unsigned char *last = arena_ + offset;

            if (have == last) {
                offset -= block_s;
                return;
            }

            if (free_n < free_s)
                stack_[free_n++] = ptr;
        }

        constexpr long size_total() const {
            return static_cast<long>(((arena_s / block_s) * block_s) + stack_s);
        }

        long size_used() const {
            return static_cast<long>(offset + (free_n * sizeof(void *)));
        }

        void clean() {
            offset = 0;
            free_n = 0;
        }
    };

    struct allocator {

        struct mem_region {
            mem_region    *prev;
            mem_region    *next;
            unsigned long  size;
            unsigned char *data;
        };

        const std::size_t block_s;
        mem_provider      memory_;

        mem_region *root_a;
        mem_region *root_s;

        mem_region *arena_;
        mem_region *stack_;

        std::size_t data_s;
        std::size_t free_s;

        int64_t total_size;
        int64_t taken_size;

        int64_t offset;
        int64_t free_n;

        ex::data::allocator compat() {
            return alloc_compat<allocator>::allocator(this);
        }

        allocator() = delete;

        allocator(const std::size_t block_size_b,
                  const std::size_t arena_size_b) :
            block_s(block_size_b),
            arena_(nullptr),
            stack_(nullptr),
            total_size(0),
            taken_size(0),
            offset(0),
            free_n(0) {
            if (block_size_b == 0)
                abort_("block_size_b cannot be 0");
            data_s = arena_size_b / block_s * block_s;
            free_s = arena_size_b / block_s;
            mem_request_();
            stack_request_();
        }

        allocator(const std::size_t block_size_b,
                  const std::size_t arena_size_b,
                  const std::size_t stack_size_b) :
            block_s(block_size_b),
            arena_(nullptr),
            stack_(nullptr),
            total_size(0),
            taken_size(0),
            offset(0),
            free_n(0) {
            if (block_size_b == 0)
                abort_("block_size_b cannot be 0");
            if (stack_size_b == 0)
                abort_("stack_size_b cannot be 0");
            data_s = arena_size_b / block_s * block_s;
            free_s = stack_size_b / sizeof(void *);
            mem_request_();
            stack_request_();
        }

        allocator(const allocator &other) = delete;
        allocator(allocator &&other)      = delete;

        allocator &operator = (const allocator &other) = delete;
        allocator &operator = (allocator &&other)      = delete;

        ~allocator() {
            memory_.clean();
        }

        void clean() {
            taken_size = 0;
            free_n     = 0;
            offset     = 0;
            arena_     = root_a;
            stack_     = root_s;
        }

        void *realloc(void *, const std::size_t) {
            abort_("Realloc not supported, this is arena bruh");
        }

        /**
         * allocates only one block at a time
         */
        void *malloc(std::size_t, std::size_t) {
            if (free_n > 0) {
                const unsigned char *raw_ptr = stack_->data + free_n - sizeof(void *);
                uintptr_t ptr_val = 0;
                for (int i = 0; i < sizeof(void *); ++i) {
                    ptr_val |= static_cast<uintptr_t>(raw_ptr[i]) << (i * 8);
                }
                void *out_ptr = reinterpret_cast<void *>(ptr_val);

                free_n -= sizeof(void *);
                if (free_n <= 0 && stack_->prev != nullptr) {
                    stack_ = stack_->prev;
                    free_n = stack_->size;
                }

                taken_size += block_s;
                return out_ptr;
            }

        try_again:
            if (offset >= arena_->size) {
                if (offset == 0)
                    abort_("offset == 0 yet no space");
                offset = 0;
                if (arena_->next != nullptr) {
                    arena_ = arena_->next;
                    goto try_again;
                }
                mem_request_();
            }

            void *ptr = arena_->data + offset;
            offset += block_s;
            taken_size += block_s;

            return ptr;
        }

        /**
         * releases block
         */
        void free(void *ptr) {
            if (static_cast<unsigned char *>(ptr) == (arena_->data + offset - block_s)) {
                offset -= block_s;
                taken_size -= block_s;
                return;
            }

        try_again:
            if (free_n >= stack_->size) {
                if (free_n == 0)
                    abort_("free_n == 0 yet no space");
                free_n = 0;
                if (stack_->next != nullptr) {
                    stack_ = stack_->next;
                    goto try_again;
                }
                stack_request_();
            }

            const uintptr_t raw_ptr_val = reinterpret_cast<uintptr_t>(ptr);
            unsigned char  *stack_ptr   = stack_->data + free_n;
            for (int i = 0; i < sizeof(void *); ++i) {
                stack_ptr[i] = (raw_ptr_val >> (i * 8)) & 0xFF;
            }

            free_n += sizeof(void *);
            taken_size -= block_s;
        }

        long size_total() const {
            return memory_.size();
        }

        long size_used() const {
            return taken_size;
        }

        void mem_request_() {
            std::size_t allocated_size = 0;
            void       *allocated_ptr  = memory_.request(sizeof(mem_region) + block_s + data_s, &allocated_size);

            mem_region *const region = reinterpret_cast<mem_region *>(allocated_ptr);
            region->prev = arena_;
            region->next = nullptr;

            unsigned char  *raw_data_ptr = static_cast<unsigned char *>(allocated_ptr) + sizeof(mem_region);
            const u_int32_t padding      = padding_(raw_data_ptr, block_s);
            unsigned char  *data_ptr     = raw_data_ptr + padding;

            region->size = allocated_size - padding - sizeof(mem_region);
            region->data = data_ptr;

            if (arena_ != nullptr)
                arena_->next = region;
            else
                root_a = region;
            arena_ = region;

            total_size += allocated_size;
        }

        void stack_request_() {
            std::size_t allocated_size = 0;
            void       *allocated_ptr  = memory_.request(sizeof(mem_region) + sizeof(void *) + free_s, &allocated_size);

            mem_region *const region = reinterpret_cast<mem_region *>(allocated_ptr);
            region->prev = stack_;
            region->next = nullptr;

            unsigned char  *raw_data_ptr = static_cast<unsigned char *>(allocated_ptr) + sizeof(mem_region);
            const u_int32_t padding      = padding_(raw_data_ptr, alignof(void *));
            unsigned char  *data_ptr     = raw_data_ptr + padding;

            region->size = allocated_size - padding - sizeof(mem_region);
            region->data = data_ptr;

            if (stack_ != nullptr)
                stack_->next = region;
            else
                root_s = region;
            stack_ = region;

            total_size += allocated_size;
        }

        static u_int32_t padding_(const void *ptr, const u_int64_t alignment_b) {
            const u_int64_t address = reinterpret_cast<u_int64_t>(ptr);
            return (alignment_b - (address % alignment_b)) % alignment_b;
        }
    };


}

#endif //EX_LIMBO_DATA_STACK_ARENA_H
