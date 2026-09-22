#pragma once

#include "flatmap.h"

// A set kept as a sorted vector of keys, searched by bisection - ceres/ds/flatmap.h with no value,
// for the same reason ceres/ds/hset.h exists next to ceres/ds/gmap.h.
//
//   struct flatset s;  fset_init(&s, sizeof(int), int_cmp);
//   int k = 3;  fset_add(&s, &k);
//   if (fset_has(&s, &k)) { ... }

struct flatset { struct flatmap map; };

static inline void fset_init(struct flatset* s, unsigned int key_size, int (*cmp)(const void*, const void*)) { fmap_init(&s->map, key_size, 0, cmp); }
static inline void fset_free(struct flatset* s) { fmap_free(&s->map); }
static inline void fset_clear(struct flatset* s) { fmap_clear(&s->map); }
static inline int  fset_add(struct flatset* s, const void* key) { return fmap_set(&s->map, key, NULL); }       // 0 ok, -1 out of memory
static inline int  fset_has(const struct flatset* s, const void* key) { return fmap_has(&s->map, key); }
static inline int  fset_remove(struct flatset* s, const void* key) { return fmap_remove(&s->map, key); }       // 1 removed, 0 was not there
static inline unsigned int fset_len(const struct flatset* s) { return fmap_len(&s->map); }

// The i-th key in order, or NULL when i >= fset_len(s).
static inline const void* fset_at(const struct flatset* s, unsigned int i) { return fmap_pair_at(&s->map, i); }
