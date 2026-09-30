// A string-to-pointer hash map, over ceres/ds/gmap.h with the string key baked in. See ceres/ds/hashmap.h.
#include "ceres/ds/hashmap.h"
#include "ceres/hash.h"
#include "stdlib.h"
#include "string.h"

// What the backing gmap stores: the owned copy of the key, then the caller's value. The gmap key itself is
// the `owned` pointer, so its hash and equality read the string it points at - and the pair keeps the
// pointer where free/remove can reach it again (gmap itself never owns or returns the key).
struct hmap_pair
{
    char* owned;
    void* value;
};

static unsigned int spread(unsigned int h)
{
    return h ^ (h >> 15) ^ (h >> 7);                    // djb2 keeps its entropy high: fold some of it down
}

static unsigned int hash_key(const void* key, unsigned int key_size)
{
    (void)key_size;
    return spread(hash_djb2(*(const char* const*)key));
}

static int eq_key(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b) == 0;
}

int hashmap_init(struct hashmap* m, unsigned int initial_cap)
{
    return gmap_init(&m->m, sizeof(char*), sizeof(struct hmap_pair), initial_cap, hash_key, eq_key);
}

static struct hmap_pair* find_pair(const struct hashmap* m, const char* key)
{
    return (struct hmap_pair*)gmap_get(&m->m, &key);
}

int hashmap_set(struct hashmap* m, const char* key, void* value)
{
    struct hmap_pair* pair = find_pair(m, key);
    if (pair != NULL)
    {
        pair->value = value;                            // an existing key: the value changes, the copy stays
        return 0;
    }
    char* copy = (char*)malloc(strlen(key) + 1u);
    if (copy == NULL)
        return -1;
    strcpy(copy, key);
    struct hmap_pair fresh = { copy, value };
    if (gmap_set(&m->m, &copy, &fresh) != 0)
    {
        free(copy);
        return -1;
    }
    return 0;
}

void* hashmap_get(const struct hashmap* m, const char* key)
{
    struct hmap_pair* pair = find_pair(m, key);
    return pair == NULL ? NULL : pair->value;
}

int hashmap_has(const struct hashmap* m, const char* key)
{
    return gmap_has(&m->m, &key);
}

int hashmap_remove(struct hashmap* m, const char* key)
{
    struct hmap_pair* pair = find_pair(m, key);
    if (pair == NULL)
        return 0;
    char* owned = pair->owned;                          // save it: gmap_remove re-finds the key, so it must
    gmap_remove(&m->m, &key);                           // still be valid while it runs
    free(owned);
    return 1;
}

static void free_pair(const void* key, void* value, void* ctx)
{
    (void)key;
    (void)ctx;
    free(((struct hmap_pair*)value)->owned);
}

void hashmap_clear(struct hashmap* m)
{
    gmap_each(&m->m, free_pair, NULL);
    gmap_clear(&m->m);
}

void hashmap_free(struct hashmap* m)
{
    hashmap_clear(m);
    gmap_free(&m->m);
}

struct each_ctx
{
    void (*fn)(const char*, void*, void*);
    void* user;
};

static void each_pair(const void* key, void* value, void* ctx)
{
    (void)key;
    struct each_ctx* c = (struct each_ctx*)ctx;
    struct hmap_pair* pair = (struct hmap_pair*)value;
    c->fn(pair->owned, pair->value, c->user);
}

void hashmap_each(const struct hashmap* m, void (*fn)(const char*, void*, void*), void* ctx)
{
    struct each_ctx c = { fn, ctx };
    gmap_each(&m->m, each_pair, &c);
}
