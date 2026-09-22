// A sorted vector of (key, value) pairs, searched by bisection. See ceres/ds/flatmap.h.
#include "ceres/ds/flatmap.h"
#include "string.h"

void fmap_init(struct flatmap* m, unsigned int key_size, unsigned int value_size, int (*cmp)(const void*, const void*))
{
    vector_init(&m->pairs, key_size + value_size);
    m->key_size = key_size;
    m->value_size = value_size;
    m->cmp = cmp;
}

void fmap_free(struct flatmap* m) { vector_free(&m->pairs); }
void fmap_clear(struct flatmap* m) { vector_clear(&m->pairs); }

// Bisects for `key`. Always sets *at to where the key is (if found) or where it belongs (if not);
// the return value is what says which case it was.
static int bisect(const struct flatmap* m, const void* key, unsigned int* at)
{
    unsigned int lo = 0, hi = m->pairs.len;
    while (lo < hi)
    {
        unsigned int mid = lo + (hi - lo) / 2u;
        const void* midKey = vector_at(&m->pairs, mid);
        int c = m->cmp(key, midKey);
        if (c == 0)
        {
            *at = mid;
            return 1;
        }
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1u;
    }
    *at = lo;
    return 0;
}

int fmap_set(struct flatmap* m, const void* key, const void* value)
{
    unsigned int at;
    if (bisect(m, key, &at))
    {
        char* pair = (char*)vector_at(&m->pairs, at);
        if (m->value_size)
            memcpy(pair + m->key_size, value, m->value_size);
        return 0;
    }
    // Not found: insert at `at`. Grown and shifted here rather than through vector_insert, which
    // wants key and value already adjacent in one blob - this way neither has to be.
    if (m->pairs.len == m->pairs.cap && vector_reserve(&m->pairs, m->pairs.cap == 0 ? 4u : m->pairs.cap * 2u) != 0)
        return -1;
    char* base = (char*)m->pairs.data;
    unsigned int elem = m->pairs.elem;
    memmove(base + (size_t)(at + 1u) * elem, base + (size_t)at * elem, (size_t)(m->pairs.len - at) * elem);
    memcpy(base + (size_t)at * elem, key, m->key_size);
    if (m->value_size)
        memcpy(base + (size_t)at * elem + m->key_size, value, m->value_size);
    m->pairs.len++;
    return 0;
}

void* fmap_get(const struct flatmap* m, const void* key)
{
    unsigned int at;
    if (!bisect(m, key, &at))
        return NULL;
    return (char*)vector_at(&m->pairs, at) + m->key_size;
}

int fmap_has(const struct flatmap* m, const void* key)
{
    unsigned int at;
    return bisect(m, key, &at);
}

int fmap_remove(struct flatmap* m, const void* key)
{
    unsigned int at;
    if (!bisect(m, key, &at))
        return 0;
    vector_remove(&m->pairs, at);
    return 1;
}

const void* fmap_pair_at(const struct flatmap* m, unsigned int i)
{
    return vector_at(&m->pairs, i);
}
