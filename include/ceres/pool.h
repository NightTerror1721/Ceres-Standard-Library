#pragma once

#include "../stddef.h"

// Fixed-size blocks with allocation and release in constant time: entities, bullets, particles. Free
// blocks form a list threaded through their own first four bytes, so a block is at least 4 bytes and is
// rounded up to a multiple of 4 to stay aligned.

struct pool
{
    char* base;
    unsigned int item;         // block size after rounding
    unsigned int count;        // how many blocks the memory holds
    unsigned int used;         // blocks handed out and not yet freed
    unsigned int fresh;        // blocks never handed out yet (taken in order from base)
    void* free_list;           // released blocks, most recent first
};

// `buf` needs item_size * count bytes (item_size rounded up to a multiple of 4), aligned to 4.
void  pool_init(struct pool* p, void* buf, unsigned int item_size, unsigned int count);
void* pool_alloc(struct pool* p);                    // NULL when every block is taken
void  pool_free(struct pool* p, void* item);         // ignores NULL and pointers that are not one of its blocks
int   pool_full(const struct pool* p);
unsigned int pool_used(const struct pool* p);
void  pool_clear(struct pool* p);                    // every block becomes free again
