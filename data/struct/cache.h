//
// Created by henryco on 12/03/25.
//

#ifndef EX_LIMBO_DATA_CACHE_H
#define EX_LIMBO_DATA_CACHE_H

#include "array.h"

namespace ex::data {

    template<typename key_t, typename val_t>
    struct cache_line {

#define READY   (0)
#define FRESH   (1 << 1)
#define EVICTED (1 << 2)

        using Functor = val_t (*)(const key_t&);

        Functor supplier;

        ex::data::array<val_t> store_data;
        ex::data::array<key_t> store_id;
        unsigned char          status = FRESH;
        unsigned int           index  = 0;

        cache_line() = delete;

        cache_line(const Functor supplier, const int reserve = 10) :
            supplier(supplier), store_data(reserve), store_id(reserve) {
        }

        val_t &operator [] (const key_t &id) {
            if (status == READY)
                return store_data[index++];

            if (status == EVICTED) {
                store_data[0] = supplier(id);
                return store_data[0];
            }

            store_id.push(id);
            store_data.push(supplier(id));
            return store_data[index];
        }

        val_t operator () (const key_t &id) {
            if (status == EVICTED)
                return supplier(id);

            if (status == FRESH) {
                const val_t &element = supplier(id);
                store_id.push(id);
                store_data.push(element);
                return element;
            }

            if (index >= store_id.size) {
                status = EVICTED;
                return supplier(id);
            }

            if (store_id[index] != id) {
                status = EVICTED;
                return supplier(id);
            }

            return store_data[index++];
        }

        void begin() {
            index = 0;
        }

        void end() {
            if (status == EVICTED) {
                evict();
                status = FRESH;
                return;
            }
            status = READY;
        }

        void evict() {
            store_data.size = 0;
            store_id.size   = 0;
            index           = 0;
            status          = EVICTED;
        }

        void reset() {
            evict();
        }
    };

}

#endif //EX_LIMBO_DATA_CACHE_H
