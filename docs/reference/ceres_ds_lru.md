# `<ceres/ds/lru.h>`

A cache of bounded size with least-recently-used eviction: ceres/ds/gmap.h for O(1) lookup by key, ceres/ds/list.h for O(1) "move to the front" and "the oldest is the back" - a texture, a decoded sprite, or a disk sector kept around only while it keeps getting used.

```c
struct lru c;  lru_init(&c, sizeof(int), sizeof(struct texture), 64, hash_int, eq_int);
int k = 3;  struct texture t = { ... };
lru_put(&c, &k, &t);
struct texture* found = (struct texture*)lru_get(&c, &k);   // NULL if not cached; moves it to the front if it is
```

Deviates from a literal "the list node lives inside what gmap stores" design: gmap.h moves every slot's bytes on a rebuild (see gmap.c), and an intrusive list_node's neighbors hold its raw address - moving it would dangle two pointers. So each entry is its own allocation at a stable address, and gmap only ever stores a POINTER to it; the list threads the entries themselves.

```c
struct lru_entry
{
    struct list_node link;             // intrusive; c->order is ordered most-recently-used first
    unsigned char data[1];             // key_size bytes of key, then value_size bytes of value
};

struct lru
{
    struct gmap index;                  // key -> struct lru_entry* (a pointer - see above)
    struct list order;
    unsigned int capacity;              // 0 means unbounded (never evicts)
    unsigned int key_size;
    unsigned int value_size;
};

int   lru_init(struct lru* c, unsigned int key_size, unsigned int value_size, unsigned int capacity,
              unsigned int (*hash)(const void*, unsigned int), int (*eq)(const void*, const void*));
void  lru_free(struct lru* c);
int   lru_put(struct lru* c, const void* key, const void* value);   // 0 ok, -1 out of memory; may evict the least recently used entry
void* lru_get(struct lru* c, const void* key);                       // NULL when not cached; found entries move to the front
int   lru_has(const struct lru* c, const void* key);                 // does not disturb the order
int   lru_remove(struct lru* c, const void* key);                    // 1 removed, 0 was not there

static inline unsigned int lru_len(const struct lru* c) { return gmap_len(&c->index); }
```
