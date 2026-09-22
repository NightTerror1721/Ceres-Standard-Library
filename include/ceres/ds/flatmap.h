#pragma once

#include "vector.h"

// A map kept as a vector of (key, value) pairs, sorted by key: for a few dozen to a few hundred
// entries - configuration, a lookup table built once and read many times - contiguous memory and a
// binary search beat a tree's pointer chasing in practice, and there are no nodes to allocate.
// The trade against ceres/ds/omap.h is explicit: fmap_get is O(log n) either way, but fmap_set is
// O(n) here (inserting in place shifts everything after it) against the tree's O(log n) - the right
// choice when writes are rare and reads or full-table walks are not.
//
//   struct flatmap m;  fmap_init(&m, sizeof(int), sizeof(int), int_cmp);
//   int k = 3, v = 30;  fmap_set(&m, &k, &v);
//   int* found = fmap_get(&m, &k);
//   fmap_free(&m);
//
// cmp compares two KEYS (not pairs), the same convention as pqueue.h's comparator: cmp(a, b) < 0
// means a sorts before b.

struct flatmap
{
    struct vector pairs;       // each element: key_size bytes of key, then value_size bytes of value
    unsigned int key_size;
    unsigned int value_size;
    int (*cmp)(const void*, const void*);
};

void  fmap_init(struct flatmap* m, unsigned int key_size, unsigned int value_size, int (*cmp)(const void*, const void*));
void  fmap_free(struct flatmap* m);
void  fmap_clear(struct flatmap* m);                                    // empties it, keeps the storage
int   fmap_set(struct flatmap* m, const void* key, const void* value);  // adds or replaces; 0 ok, -1 out of memory
void* fmap_get(const struct flatmap* m, const void* key);               // pointer to the value, or NULL when absent
int   fmap_has(const struct flatmap* m, const void* key);
int   fmap_remove(struct flatmap* m, const void* key);                  // 1 removed, 0 was not there

static inline unsigned int fmap_len(const struct flatmap* m) { return m->pairs.len; }

// The i-th pair in key order (key_size bytes of key immediately followed by value_size of value),
// or NULL when i >= fmap_len(m). Walking 0 .. fmap_len(m)-1 visits every entry sorted by key.
const void* fmap_pair_at(const struct flatmap* m, unsigned int i);
