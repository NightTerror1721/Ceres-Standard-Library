# `<ceres/ds/omap.h>`

An ordered map of copies, keyed and valued by anything of a fixed size: the convenience layer over ceres/ds/rbtree.h, the way ceres/ds/pqueue.h is the convenience layer over vector.h for a heap - no need to declare a struct with an rb_node embedded in it just to hold one int key.

```c
struct omap m;  omap_init(&m, sizeof(int), sizeof(int), cmp_int);
int k = 3, v = 30;  omap_set(&m, &k, &v);
int* found = (int*)omap_get(&m, &k);
omap_free(&m);
```

Each entry is its own small allocation (key_size + value_size bytes plus an rb_node), unlike rbtree.h itself which allocates nothing - this is exactly the trade the header comment above promises. A lookup that cannot even allocate its own tiny search probe reports "not found" rather than a separate out-of-memory case: nothing later in the call could tell the two apart from a bare pointer anyway (see omap.c).

```c
struct omap
{
    struct rbtree tree;
    unsigned int key_size;
    unsigned int value_size;
    int (*cmp)(const void* a, const void* b);   // compares two KEYS, pqueue.h's convention
};

void  omap_init(struct omap* m, unsigned int key_size, unsigned int value_size, int (*cmp)(const void* a, const void* b));
void  omap_free(struct omap* m);
int   omap_set(struct omap* m, const void* key, const void* value);   // adds or replaces; 0 ok, -1 out of memory
void* omap_get(const struct omap* m, const void* key);                // pointer to the stored value, or NULL
int   omap_has(const struct omap* m, const void* key);
int   omap_remove(struct omap* m, const void* key);                   // 1 removed, 0 was not there

static inline unsigned int omap_len(const struct omap* m) { return rb_count(&m->tree); }

// In-order walk of the KEYS, smallest first: omap_first_key, then omap_next_key from whatever the
// last call returned, until NULL. `key` must be a KEY pointer this same omap already handed back
// (from omap_first_key or omap_next_key, not omap_get's value pointer) - the walk finds its place
// from it directly rather than re-searching by value, so it stays O(1) per step even though the
// API only ever deals in keys, never a node type of its own. Call omap_get(m, key) for the value
// that goes with a key the walk just produced.
const void* omap_first_key(const struct omap* m);
const void* omap_next_key(const struct omap* m, const void* key);
```
