# `<ceres/ds/oset.h>`

An ordered set of copies - ceres/ds/omap.h with no value, the tree-backed counterpart to ceres/ds/hset.h and ceres/ds/flatset.h.

```c
struct oset s;  oset_init(&s, sizeof(int), cmp_int);
int k = 3;  oset_add(&s, &k);
if (oset_has(&s, &k)) { ... }
for (const void* k = oset_first(&s); k != NULL; k = oset_next(&s, k)) ...   // smallest first
```

```c
struct oset { struct omap map; };

static inline void oset_init(struct oset* s, unsigned int key_size, int (*cmp)(const void*, const void*)) { omap_init(&s->map, key_size, 0, cmp); }
static inline void oset_free(struct oset* s) { omap_free(&s->map); }
static inline int  oset_add(struct oset* s, const void* key) { return omap_set(&s->map, key, NULL); }   // 0 ok, -1 out of memory
static inline int  oset_has(const struct oset* s, const void* key) { return omap_has(&s->map, key); }
static inline int  oset_remove(struct oset* s, const void* key) { return omap_remove(&s->map, key); }
static inline unsigned int oset_len(const struct oset* s) { return omap_len(&s->map); }
static inline const void* oset_first(const struct oset* s) { return omap_first_key(&s->map); }
static inline const void* oset_next(const struct oset* s, const void* key) { return omap_next_key(&s->map, key); }
```
