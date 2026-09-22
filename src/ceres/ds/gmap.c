// Generic-key hash map, open addressing. See ceres/ds/gmap.h.
#include "ceres/ds/gmap.h"
#include "ceres/bits.h"
#include "stdlib.h"
#include "string.h"

enum { SLOT_EMPTY = 0, SLOT_USED = 1, SLOT_TOMBSTONE = 2 };

static void* pair_at(const struct gmap* m, unsigned int i)
{
    return (char*)m->pairs + (size_t)i * (m->key_size + m->value_size);
}

static int table_alloc(struct gmap* m, unsigned int cap)
{
    unsigned char* states = (unsigned char*)calloc(cap, 1);                        // SLOT_EMPTY == 0
    unsigned int* hashes = (unsigned int*)malloc((size_t)cap * sizeof(unsigned int));
    void* pairs = malloc((size_t)cap * (m->key_size + m->value_size));
    // malloc(0) may legitimately return NULL, so only a genuinely nonzero request failing counts.
    if (states == NULL || hashes == NULL || (pairs == NULL && (m->key_size + m->value_size) != 0))
    {
        free(states);
        free(hashes);
        free(pairs);
        return -1;
    }
    m->states = states;
    m->hashes = hashes;
    m->pairs = pairs;
    m->cap = cap;
    m->used = 0;
    return 0;
}

int gmap_init(struct gmap* m, unsigned int key_size, unsigned int value_size, unsigned int initial_cap,
    unsigned int (*hash)(const void*, unsigned int), int (*eq)(const void*, const void*))
{
    m->key_size = key_size;
    m->value_size = value_size;
    m->hash = hash;
    m->eq = eq;
    m->len = 0;
    m->states = NULL;
    m->hashes = NULL;
    m->pairs = NULL;
    m->cap = 0;
    m->used = 0;
    unsigned int cap = initial_cap < 8u ? 8u : bit_next_pow2(initial_cap);
    return table_alloc(m, cap);
}

void gmap_free(struct gmap* m)
{
    free(m->states);
    free(m->hashes);
    free(m->pairs);
    m->states = NULL;
    m->hashes = NULL;
    m->pairs = NULL;
    m->cap = 0;
    m->len = 0;
    m->used = 0;
}

// The slot index holding `key`, or -1. `h` is the already-computed hash of the key.
static int find(const struct gmap* m, const void* key, unsigned int h)
{
    if (m->cap == 0)
        return -1;
    unsigned int mask = m->cap - 1u;
    unsigned int i = h & mask;
    for (unsigned int probes = 0; probes < m->cap; probes++)
    {
        unsigned char state = m->states[i];
        if (state == SLOT_EMPTY)
            return -1;                                       // the chain ends at a never-used slot
        if (state == SLOT_USED && m->hashes[i] == h && m->eq(pair_at(m, i), key))
            return (int)i;
        i = (i + 1u) & mask;
    }
    return -1;
}

static int rebuild(struct gmap* m, unsigned int cap)
{
    struct gmap old = *m;
    if (table_alloc(m, cap) != 0)
    {
        *m = old;
        return -1;
    }
    unsigned int mask = cap - 1u;
    for (unsigned int i = 0; i < old.cap; i++)
    {
        if (old.states[i] != SLOT_USED)
            continue;
        unsigned int at = old.hashes[i] & mask;
        while (m->states[at] != SLOT_EMPTY)
            at = (at + 1u) & mask;
        m->states[at] = SLOT_USED;
        m->hashes[at] = old.hashes[i];
        memcpy(pair_at(m, at), (char*)old.pairs + (size_t)i * (m->key_size + m->value_size), m->key_size + m->value_size);
        m->used++;
    }
    free(old.states);
    free(old.hashes);
    free(old.pairs);
    return 0;
}

int gmap_set(struct gmap* m, const void* key, const void* value)
{
    unsigned int h = m->hash(key, m->key_size);
    int hit = find(m, key, h);
    if (hit >= 0)
    {
        if (m->value_size)
            memcpy((char*)pair_at(m, (unsigned int)hit) + m->key_size, value, m->value_size);
        return 0;
    }

    // Grow at 70% full, counting tombstones the same way hashmap.c does - they lengthen a probe
    // chain exactly as much as a live entry does.
    if (m->cap == 0 || (m->used + 1u) * 10u > m->cap * 7u)
    {
        unsigned int cap = m->cap == 0 ? 8u : m->cap;
        if ((m->len + 1u) * 10u > cap * 35u / 10u)
            cap *= 2u;
        if (rebuild(m, cap) != 0)
            return -1;
    }

    unsigned int mask = m->cap - 1u;
    unsigned int i = h & mask;
    while (m->states[i] == SLOT_USED)
        i = (i + 1u) & mask;
    if (m->states[i] == SLOT_EMPTY)
        m->used++;                                          // a reused tombstone is already counted
    m->states[i] = SLOT_USED;
    m->hashes[i] = h;
    char* pair = (char*)pair_at(m, i);
    memcpy(pair, key, m->key_size);
    if (m->value_size)
        memcpy(pair + m->key_size, value, m->value_size);
    m->len++;
    return 0;
}

void* gmap_get(const struct gmap* m, const void* key)
{
    int hit = find(m, key, m->hash(key, m->key_size));
    if (hit < 0)
        return NULL;
    return (char*)pair_at(m, (unsigned int)hit) + m->key_size;
}

int gmap_has(const struct gmap* m, const void* key)
{
    return find(m, key, m->hash(key, m->key_size)) >= 0;
}

int gmap_remove(struct gmap* m, const void* key)
{
    int hit = find(m, key, m->hash(key, m->key_size));
    if (hit < 0)
        return 0;
    m->states[(unsigned int)hit] = SLOT_TOMBSTONE;
    m->len--;
    return 1;
}

void gmap_clear(struct gmap* m)
{
    memset(m->states, SLOT_EMPTY, m->cap);
    m->len = 0;
    m->used = 0;
}

void gmap_each(const struct gmap* m, void (*fn)(const void* key, void* value, void* ctx), void* ctx)
{
    for (unsigned int i = 0; i < m->cap; i++)
    {
        if (m->states[i] != SLOT_USED)
            continue;
        char* pair = (char*)pair_at(m, i);
        fn(pair, m->value_size ? (pair + m->key_size) : NULL, ctx);
    }
}
