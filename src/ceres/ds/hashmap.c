// String-keyed hash map, open addressing. See ceres/ds/hashmap.h.
#include "ceres/ds/hashmap.h"
#include "ceres/hash.h"
#include "ceres/bits.h"
#include "stdlib.h"
#include "string.h"

// The address of this byte is the marker for a removed entry. Nothing ever dereferences it.
static char tombstone_byte;
#define TOMBSTONE (&tombstone_byte)

static unsigned int spread(unsigned int h)
{
    return h ^ (h >> 15) ^ (h >> 7);                    // djb2 keeps its entropy high: fold some of it down
}

static int table_alloc(struct hashmap* m, unsigned int cap)
{
    struct hashmap_slot* slots = (struct hashmap_slot*)calloc(cap, sizeof(struct hashmap_slot));
    if (slots == NULL)
        return -1;
    m->slots = slots;
    m->cap = cap;
    m->used = 0;
    return 0;
}

int hashmap_init(struct hashmap* m, unsigned int initial_cap)
{
    unsigned int cap = initial_cap < 8u ? 8u : bit_next_pow2(initial_cap);
    m->len = 0;
    m->slots = NULL;
    m->cap = 0;
    m->used = 0;
    return table_alloc(m, cap);
}

void hashmap_free(struct hashmap* m)
{
    if (m->slots != NULL)
    {
        for (unsigned int i = 0; i < m->cap; i++)
            if (m->slots[i].key != NULL && m->slots[i].key != TOMBSTONE)
                free(m->slots[i].key);
        free(m->slots);
    }
    m->slots = NULL;
    m->cap = 0;
    m->len = 0;
    m->used = 0;
}

// The slot holding `key`, or NULL. `hash` is the spread hash of the key.
static struct hashmap_slot* find(const struct hashmap* m, const char* key, unsigned int hash)
{
    if (m->cap == 0)
        return NULL;
    unsigned int mask = m->cap - 1u;
    unsigned int i = hash & mask;
    for (unsigned int probes = 0; probes < m->cap; probes++)
    {
        struct hashmap_slot* s = &m->slots[i];
        if (s->key == NULL)
            return NULL;                                // the chain ends at a never-used slot
        if (s->key != TOMBSTONE && s->hash == hash && strcmp(s->key, key) == 0)
            return s;
        i = (i + 1u) & mask;
    }
    return NULL;
}

// Moves every entry into a fresh table of `cap` slots, dropping the tombstones.
static int rebuild(struct hashmap* m, unsigned int cap)
{
    struct hashmap old = *m;
    if (table_alloc(m, cap) != 0)
    {
        *m = old;
        return -1;
    }
    unsigned int mask = cap - 1u;
    for (unsigned int i = 0; i < old.cap; i++)
    {
        struct hashmap_slot* s = &old.slots[i];
        if (s->key == NULL || s->key == TOMBSTONE)
            continue;
        unsigned int at = s->hash & mask;
        while (m->slots[at].key != NULL)
            at = (at + 1u) & mask;
        m->slots[at] = *s;
        m->used++;
    }
    free(old.slots);
    return 0;
}

int hashmap_set(struct hashmap* m, const char* key, void* value)
{
    unsigned int hash = spread(hash_djb2(key));
    struct hashmap_slot* hit = find(m, key, hash);
    if (hit != NULL)
    {
        hit->value = value;
        return 0;
    }

    // Grow at 70% full, counting tombstones: they lengthen probes as much as live entries do. When
    // most of that is tombstones a rebuild at the same size is enough.
    if (m->cap == 0 || (m->used + 1u) * 10u > m->cap * 7u)
    {
        unsigned int cap = m->cap == 0 ? 8u : m->cap;
        if ((m->len + 1u) * 10u > cap * 35u / 10u)      // more than 35% live: a bigger table
            cap *= 2u;
        if (rebuild(m, cap) != 0)
            return -1;
    }

    size_t n = strlen(key) + 1;
    char* copy = (char*)malloc(n);
    if (copy == NULL)
        return -1;
    memcpy(copy, key, n);

    unsigned int mask = m->cap - 1u;
    unsigned int i = hash & mask;
    while (m->slots[i].key != NULL && m->slots[i].key != TOMBSTONE)
        i = (i + 1u) & mask;
    if (m->slots[i].key == NULL)
        m->used++;                                      // a reused tombstone is already counted
    m->slots[i].key = copy;
    m->slots[i].value = value;
    m->slots[i].hash = hash;
    m->len++;
    return 0;
}

void* hashmap_get(const struct hashmap* m, const char* key)
{
    struct hashmap_slot* s = find(m, key, spread(hash_djb2(key)));
    return s == NULL ? NULL : s->value;
}

int hashmap_has(const struct hashmap* m, const char* key)
{
    return find(m, key, spread(hash_djb2(key))) != NULL;
}

int hashmap_remove(struct hashmap* m, const char* key)
{
    struct hashmap_slot* s = find(m, key, spread(hash_djb2(key)));
    if (s == NULL)
        return 0;
    free(s->key);
    s->key = TOMBSTONE;
    s->value = NULL;
    m->len--;
    return 1;
}

void hashmap_clear(struct hashmap* m)
{
    for (unsigned int i = 0; i < m->cap; i++)
    {
        if (m->slots[i].key != NULL && m->slots[i].key != TOMBSTONE)
            free(m->slots[i].key);
        m->slots[i].key = NULL;
        m->slots[i].value = NULL;
    }
    m->len = 0;
    m->used = 0;
}

void hashmap_each(const struct hashmap* m, void (*fn)(const char* key, void* value, void* ctx), void* ctx)
{
    for (unsigned int i = 0; i < m->cap; i++)
    {
        const struct hashmap_slot* s = &m->slots[i];
        if (s->key != NULL && s->key != TOMBSTONE)
            fn(s->key, s->value, ctx);
    }
}
