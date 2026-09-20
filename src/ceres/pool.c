// Fixed-size block pool. See ceres/pool.h.
#include "ceres/pool.h"

void pool_init(struct pool* p, void* buf, unsigned int item_size, unsigned int count)
{
    if (item_size < 4u)
        item_size = 4u;
    p->base = (char*)buf;
    p->item = (item_size + 3u) & ~3u;
    p->count = count;
    p->used = 0;
    p->fresh = count;
    p->free_list = NULL;
}

void* pool_alloc(struct pool* p)
{
    if (p->free_list != NULL)
    {
        void* block = p->free_list;
        p->free_list = *(void**)block;
        p->used++;
        return block;
    }
    if (p->fresh == 0)
        return NULL;
    void* block = p->base + (p->count - p->fresh) * p->item;
    p->fresh--;
    p->used++;
    return block;
}

void pool_free(struct pool* p, void* item)
{
    if (item == NULL)
        return;
    unsigned int offset = (unsigned int)item - (unsigned int)p->base;
    if ((unsigned int)item < (unsigned int)p->base || offset >= p->count * p->item || offset % p->item != 0)
        return;
    *(void**)item = p->free_list;
    p->free_list = item;
    p->used--;
}

int pool_full(const struct pool* p)
{
    return p->free_list == NULL && p->fresh == 0;
}

unsigned int pool_used(const struct pool* p)
{
    return p->used;
}

void pool_clear(struct pool* p)
{
    p->used = 0;
    p->fresh = p->count;
    p->free_list = NULL;
}
