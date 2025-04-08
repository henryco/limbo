//
// Created by henryco on 18/03/25.
//

#ifndef EX_LIMBO_DATA_MEMPROVIDER_H
#define EX_LIMBO_DATA_MEMPROVIDER_H

#if defined(__linux__)

#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>

#include "datalloc.h"

#ifdef MEM_PROVIDER_VALGRIND
        #include <valgrind/memcheck.h>
// TODO
#else
// TODO
#endif

namespace ex::data {

    struct mem_provider {

        struct alignas(32) mem_node {
            void     *data;
            mem_node *prev;
            mem_node *orgn;
            int64_t   size;
        };

        int64_t   node_num_;
        int64_t   node_ctr_;

        int64_t   page_size;
        mem_node *node_head;

        mem_provider() :
            node_num_(0),
            node_ctr_(0),
            page_size(sysconf(_SC_PAGESIZE)),
            node_head(nullptr) {
            if (page_size == -1) {
                abort_("page_size read error");
            }
        }

        ~mem_provider() {
            if (node_head == nullptr)
                return;
            clean();
        }

        void clean() {
            mem_node *head = node_head;
            while (head != nullptr) {
                mem_node *prev = head->prev;
                if (head->data != nullptr && head->size > 0) {
                    if (const int err = ::munmap(head->data, head->size); err != 0)
                        abort_("munmap deallocation error: " << err);
                }
                if (head->orgn != nullptr) {
                    if (const int err = ::munmap(head->orgn, page_size); err != 0)
                        abort_("munmap deallocation error: " << err);
                }
                head = prev;
            }
            node_head = nullptr;
            node_num_ = 0;
            node_ctr_ = 0;
        }

        long size() const {
            // TODO FIXME
            long            bytes = 0;
            const mem_node *head  = node_head;
            while (head != nullptr) {
                bytes += head->size;
                head = head->prev;
            }
            return bytes;
        }

        void *request(const std::size_t size, std::size_t *size_allocated = nullptr) {
            const std::size_t need_size = size;
            const std::size_t real_size = (need_size % page_size != 0)
                ? (need_size - (need_size % page_size) + page_size)
                : (need_size);

            constexpr int prot = PROT_READ | PROT_WRITE;
            constexpr int flag = MAP_PRIVATE | MAP_ANONYMOUS;

            mem_node *node = req_node_();

            void *mem = ::mmap(nullptr, real_size, prot, flag, -1, 0);

            if (mem == MAP_FAILED)
                abort_("mmap page allocation failed [MAP_FAILED]");

            const int advice = MADV_WILLNEED | ( real_size >= (2 * 1024 * 1024) ? MADV_HUGEPAGE : 0);
            ::madvise(mem, real_size, advice);

            node->size = static_cast<int64_t>(real_size);
            node->data = mem;

            *size_allocated = real_size;
            return mem;
        }

        void release(void *ptr) {
            if (ptr == nullptr)
                return;
            mem_node *head = node_head;
            while (head != nullptr) {
                mem_node *prev = head->prev;
                if (head->data == ptr) {
                    if (head->size > 0 && head->data != nullptr) {
                        if (const int err = ::munmap(head->data, head->size); err != 0)
                            abort_("munmap deallocation error: " << err);
                    }
                    head->size = 0;
                    head->data = nullptr;
                    // release and create gap
                    return;
                }
                head = prev;
            }
        }

        mem_node *req_node_() {
            if (node_head == nullptr || node_num_ <= 0 || node_ctr_ >= node_num_) {
                constexpr int prot = PROT_READ | PROT_WRITE;
                constexpr int flag = MAP_PRIVATE | MAP_ANONYMOUS;
                void *mem = mmap(nullptr, page_size, prot, flag, -1, 0);

                if (mem == MAP_FAILED)
                    abort_("mmap page allocation failed [MAP_FAILED]");

                ::madvise(mem, page_size, MADV_WILLNEED);

                const u_int32_t padding  = padding_(mem, alignof(mem_node));
                unsigned char  *work_ptr = static_cast<unsigned char *>(mem) + padding;
                mem_node       *region   = reinterpret_cast<mem_node *>(work_ptr);

                region->orgn = region;

                if (node_head != nullptr)
                    region->prev = node_head;

                node_num_ = static_cast<int64_t>((page_size - padding) / sizeof(mem_node));
                node_ctr_ = 1;
                node_head = region;
                return node_head;
            }

            mem_node *region = node_head + 1;
            region->prev     = node_head;

            node_ctr_ += 1;
            node_head = region;
            return node_head;
        }

        static u_int32_t padding_(const void *ptr, const u_int64_t alignment_b) {
            const u_int64_t address = reinterpret_cast<u_int64_t>(ptr);
            return (alignment_b - (address % alignment_b)) % alignment_b;
        }
    };
}

#else

// Non linux fallback to <cstdlib> malloc
#include "datalloc.h"
#include <iostream>
#include <cstdlib>
namespace ex::data {

    struct mem_provider {

        struct mem_node {
            mem_node *prev;
            void     *data;
            long      size;
        };

        mem_node *node = nullptr;

        mem_provider() = default;

        ~mem_provider() {
            if (node == nullptr)
                return;
            mem_node *head = node;
            while (head != nullptr) {
                mem_node *prev = head->prev;
                ::free(head->data);
                ::free(head);
                head = prev;
            }
            node = nullptr;
        }

        void clean() {
            if (node == nullptr)
                return;
            mem_node *head = node;
            while (head != nullptr) {
                mem_node *prev = head->prev;
                ::free(head->data);
                ::free(head);
                head = prev;
            }
            node = nullptr;
        }

        long size() const {
            long            s    = 0;
            const mem_node *head = node;
            while (head != nullptr) {
                s += head->size;
                head = head->prev;
            }
            return s;
        }

        void *request(const std::size_t size, std::size_t *size_allocated = nullptr) {
            const std::size_t real_size = size;

            void *mem = ::malloc(real_size);
            if (mem == nullptr)
                abort_("Memory allocation error");

            if (node == nullptr) {
                node = static_cast<mem_node *>(::malloc(sizeof(mem_node)));
                if (node == nullptr)
                    abort_("Memory allocation error");
                node->prev = nullptr;
                node->data = mem;
                node->size = real_size;
            } else {
                mem_node *new_node = static_cast<mem_node *>(::malloc(sizeof(mem_node)));
                if (new_node == nullptr)
                    abort_("Memory allocation error");
                new_node->prev     = node;
                new_node->data     = mem;
                new_node->size     = real_size;
                node               = new_node;
            }

            *size_allocated = real_size;
            return mem;
        }

        void release(void *ptr) {
            mem_node *head = node;
            mem_node *next = nullptr;
            while (head != nullptr) {
                mem_node *prev = head->prev;
                if (head->data == ptr) {
                    ::free(head->data);
                    if (next != nullptr)
                        next->prev = prev;
                    ::free(head);
                    return;
                }
                next = head;
                head = prev;
            }
        }
    };
}
#endif

#endif //EX_LIMBO_DATA_MEMPROVIDER_H
