# `<ceres/ds/hset.h>`

A set of keys of any fixed size - ceres/ds/gmap.h with no value, the hash-table counterpart to ceres/ds/flatset.h. "Have I already seen this (x, y)?", "is this entity already in this frame's collision list?" - anything whose key is not a string, which ceres/ds/hashmap.h cannot hold.

```c
struct hset s;  hset_init(&s, sizeof(int), 0, hash_int, eq_int);
int k = 3;  hset_add(&s, &k);
if (hset_has(&s, &k)) { ... }
```

```c
struct hset { struct gmap map; };

static inline int hset_init(struct hset* s, unsigned int key_size, unsigned int initial_cap,
    unsigned int (*hash)(const void*, unsigned int), int (*eq)(const void*, const void*))
{
    return gmap_init(&s->map, key_size, 0, initial_cap, hash, eq);
}
static inline void hset_free(struct hset* s) { gmap_free(&s->map); }
static inline void hset_clear(struct hset* s) { gmap_clear(&s->map); }
static inline int  hset_has(const struct hset* s, const void* key) { return gmap_has(&s->map, key); }
static inline int  hset_remove(struct hset* s, const void* key) { return gmap_remove(&s->map, key); }
static inline unsigned int hset_len(const struct hset* s) { return gmap_len(&s->map); }

// 1 when the key was new, 0 when it was already there, -1 when out of memory.
static inline int hset_add(struct hset* s, const void* key)
{
    if (gmap_has(&s->map, key))
        return 0;
    return gmap_set(&s->map, key, NULL) == 0 ? 1 : -1;
}

// `fn` is called with each key and a NULL value, for symmetry with gmap_each.
static inline void hset_each(const struct hset* s, void (*fn)(const void* key, void* value, void* ctx), void* ctx)
{
    gmap_each(&s->map, fn, ctx);
}
```
