#pragma once

#include "gmap.h"

// A hash map from strings to pointers: ceres/ds/gmap.h with the string key baked in. The map keeps its
// own copy of every key; the values are the caller's pointers and are never followed or freed. Built on
// gmap, so the open addressing, the tombstones and the growth are the same code as every other keyed map
// here; this header keeps the string-to-pointer shape its callers already use.

struct hashmap
{
    struct gmap m;             // key: char* (the owned copy), value: { char* owned; void* value }
};

int   hashmap_init(struct hashmap* m, unsigned int initial_cap);      // 0 ok, -1 when out of memory (cap is rounded up to a power of two, at least 8)
void  hashmap_free(struct hashmap* m);                                 // frees the keys and the table, not the values
int   hashmap_set(struct hashmap* m, const char* key, void* value);    // adds or replaces; 0 ok, -1 when out of memory
void* hashmap_get(const struct hashmap* m, const char* key);           // the value, or NULL when there is no such key
int   hashmap_has(const struct hashmap* m, const char* key);           // 1 when the key is present, even with a NULL value
int   hashmap_remove(struct hashmap* m, const char* key);              // 1 when it was there, 0 when not
void  hashmap_clear(struct hashmap* m);                                // removes every entry, keeps the table
void  hashmap_each(const struct hashmap* m, void (*fn)(const char* key, void* value, void* ctx), void* ctx);   // in no particular order

static inline unsigned int hashmap_len(const struct hashmap* m) { return m->m.len; }
