//
// Created by henryco on 01/03/25.
//

#ifndef EX_DATALLOC_H
#define EX_DATALLOC_H

#include <iostream>
#include <cstdlib>

namespace ex::data {

#ifndef UNIT_TEST
    #define abort_(msg) do { std::cerr << msg << std::endl; ::abort(); } while(false)
#else
    #define abort_(msg) throw std::runtime_error("Generic abort exception")
#endif

#define KB * 1024UL
#define MB * 1024UL KB
#define GB * 1024UL MB
#define TB * 1024UL GB

    using instance_t   = void *;
    using malloc_fn_t  = void *(*) (instance_t this_, std::size_t size_new_b, std::size_t alignment_b);
    using realloc_fn_t = void *(*) (instance_t this_, void *ptr, std::size_t size_new_b);
    using free_fn_t    = void  (*) (instance_t this_, void *ptr);
    using size_fn_t    = long  (*) (instance_t this_);

    inline std::size_t default_size_ctr_ = 0;

    inline void *default_malloc_(instance_t, const std::size_t size_new_b, std::size_t) {
        void *mem = ::malloc(size_new_b);
        if (mem == nullptr)
            ::abort();
        default_size_ctr_ += size_new_b + 16;
        return mem;
    }

    inline void *default_realloc_(instance_t, void *ptr, const std::size_t size_new_b) {
        void *mem = ::realloc(ptr, size_new_b);
        if (mem == nullptr)
            ::abort();
        default_size_ctr_ += size_new_b + 16;
        return mem;
    }

    inline void default_free_(instance_t, void *ptr) {
        ::free(ptr);
    }

    inline long default_size_(instance_t) {
        return default_size_ctr_;
    }

    struct allocator {
        malloc_fn_t  malloc      = &default_malloc_;
        realloc_fn_t realloc     = &default_realloc_;
        free_fn_t    free        = &default_free_;
        size_fn_t    size_total  = &default_size_;
        size_fn_t    size_used   = &default_size_;
        instance_t   instance    = nullptr;
    };

    template <typename T>
    struct alloc_compat {

        static ex::data::allocator allocator(T &this_) {
            return allocator(&this_);
        }

        static ex::data::allocator allocator(T *this_) {
            return { .malloc      = &alloc_compat::malloc_,
                     .realloc     = &alloc_compat::realloc_,
                     .free        = &alloc_compat::free_,
                     .size_total  = &alloc_compat::size_total_,
                     .size_used   = &alloc_compat::size_used_,
                     .instance    = this_ };
        }

        static void *malloc_(void *this_, const std::size_t size_new_b, const std::size_t alignment_b) {
            return static_cast<T *>(static_cast<const ex::data::allocator *>(this_)->instance)->malloc(size_new_b, alignment_b);
        }

        static void *realloc_(void *this_, void *ptr, const std::size_t size_new_b) {
            return static_cast<T *>(static_cast<const ex::data::allocator *>(this_)->instance)->realloc(ptr, size_new_b);
        }

        static void free_(void *this_, void *ptr) {
            static_cast<T *>(static_cast<const ex::data::allocator *>(this_)->instance)->free(ptr);
        }

        static long size_total_(void *this_) {
            return static_cast<T *>(static_cast<const ex::data::allocator *>(this_)->instance)->size_total();
        }

        static long size_used_(void *this_) {
            return static_cast<T *>(static_cast<const ex::data::allocator *>(this_)->instance)->size_used();
        }
    };

}

#endif //EX_DATALLOC_H
