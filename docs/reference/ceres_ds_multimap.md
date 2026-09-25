# `<ceres/ds/multimap.h>`

A map from one key to any number of values - an event's list of listeners, a name's list of adjacent nodes - which neither ceres/ds/hashmap.h nor ceres/ds/gmap.h can hold (one key, one value each). Each key in the underlying gmap points at the head of a chain of value nodes instead of at a value directly; a node is reserved with plain malloc/free rather than ceres/pool.h, because pool.h needs its total block count known up front and a chain's length here is not - the node-with-pointer style (see the collections research) with an ordinary allocator behind it instead of a fixed arena.

```c
struct multimap m;  mm_init(&m, sizeof(int), sizeof(int), hash_int, eq_int);
int k = 1, a = 10, b = 20;
mm_add(&m, &k, &a);  mm_add(&m, &k, &b);
mm_each(&m, &k, print_value, NULL);         // sees 20 then 10 - most recent first
mm_free(&m);
```

```c
struct mm_chain { struct mm_chain* next; unsigned char value[1]; };   // real size: offsetof(value) + value_size

struct multimap
{
    struct gmap keys;          // each key's value is a `struct mm_chain*` - the chain's head
    unsigned int value_size;
};

int  mm_init(struct multimap* m, unsigned int key_size, unsigned int value_size, unsigned int initial_cap,
            unsigned int (*hash)(const void*, unsigned int), int (*eq)(const void*, const void*));
void mm_free(struct multimap* m);
void mm_clear(struct multimap* m);                                        // removes every key and value, keeps the table
int  mm_add(struct multimap* m, const void* key, const void* value);      // always adds, never replaces; 0 ok, -1 out of memory
int  mm_remove(struct multimap* m, const void* key, const void* value);   // removes one matching value (memcmp); 1 removed, 0 not found
void mm_each(const struct multimap* m, const void* key, void (*fn)(const void* value, void* ctx), void* ctx);
unsigned int mm_count(const struct multimap* m, const void* key);          // how many values this key has, 0 if none
static inline unsigned int mm_key_count(const struct multimap* m) { return gmap_len(&m->keys); }
```
