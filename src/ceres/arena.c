// Linear allocator. See ceres/arena.h.
#include "ceres/arena.h"
#include "stdlib.h"
#include "string.h"

void arena_init(struct arena* a, void* buf, unsigned int size)
{
    a->base = (char*)buf;
    a->size = size;
    a->used = 0;
    a->owned = 0;
}

int arena_init_heap(struct arena* a, unsigned int size)
{
    void* mem = malloc(size);
    if (mem == NULL)
    {
        a->base = NULL;
        a->size = 0;
        a->used = 0;
        a->owned = 0;
        return -1;
    }
    arena_init(a, mem, size);
    a->owned = 1;
    return 0;
}

void* arena_alloc(struct arena* a, unsigned int n)
{
    // Align the ADDRESS, not the offset: the caller's buffer may itself start anywhere.
    unsigned int start = (unsigned int)a->base + a->used;
    unsigned int pad = (8u - (start & 7u)) & 7u;
    if (n > a->size || pad > a->size - a->used || n > a->size - a->used - pad)
        return NULL;
    a->used += pad;
    void* p = a->base + a->used;
    a->used += n;
    return p;
}

void* arena_alloc_zero(struct arena* a, unsigned int n)
{
    void* p = arena_alloc(a, n);
    if (p != NULL)
        memset(p, 0, n);
    return p;
}

char* arena_strdup(struct arena* a, const char* s)
{
    unsigned int n = (unsigned int)strlen(s) + 1u;
    char* p = (char*)arena_alloc(a, n);
    if (p != NULL)
        memcpy(p, s, n);
    return p;
}

unsigned int arena_mark(const struct arena* a)
{
    return a->used;
}

void arena_release(struct arena* a, unsigned int mark)
{
    if (mark <= a->used)                // a mark from the future would grow it over unwritten memory
        a->used = mark;
}

void arena_reset(struct arena* a)
{
    a->used = 0;
}

unsigned int arena_left(const struct arena* a)
{
    return a->size - a->used;
}

void arena_destroy(struct arena* a)
{
    if (a->owned)
        free(a->base);
    a->base = NULL;
    a->size = 0;
    a->used = 0;
    a->owned = 0;
}
