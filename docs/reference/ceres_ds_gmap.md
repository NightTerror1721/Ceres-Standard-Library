# `<ceres/ds/gmap.h>`

A hash map from a key of any fixed size to a value of any fixed size - ceres/ds/hashmap.h widened past "string to pointer". Open addressing with linear probing, the same shape hashmap.c already uses (a tombstone for a removed slot, so a probe chain is never cut short), generalized: a slot's "used" state is a byte of its own instead of a magic key pointer, because the key here is bytes embedded in the table, not a pointer to something allocated separately. Both the key and the value are copied in and out; the map never follows or frees anything the caller gave it a pointer to. hashmap.h stays the right choice when the key already is a string - one function pointer fewer to carry around per probe.

```c
struct gmap m;  gmap_init(&m, sizeof(int), sizeof(struct asset), 0, hash_int, eq_int);
int k = 3;  struct asset a = { ... };
gmap_set(&m, &k, &a);
struct asset* found = (struct asset*)gmap_get(&m, &k);
gmap_free(&m);
```

`hash` hashes key_size bytes at the given pointer (or whatever the key actually is, for a key like char* whose bytes are not its content); `eq` tells two keys apart the same way pqueue.h's comparator does, just for equality instead of order.

```c
struct gmap
{
    unsigned char* states;     // cap bytes: 0 empty, 1 used, 2 tombstone
    unsigned int* hashes;      // cap words, one per slot - avoids re-hashing a key on every probe
    void* pairs;                // cap * (key_size + value_size) bytes: key_size of key, then value_size of value
    unsigned int cap;           // a power of two
    unsigned int len;           // entries stored
    unsigned int used;          // slots that are not empty: entries plus tombstones
    unsigned int key_size;
    unsigned int value_size;
    unsigned int (*hash)(const void* key, unsigned int key_size);
    int (*eq)(const void* a, const void* b);
};

// `initial_cap` is rounded up to a power of two, at least 8. 0 ok, -1 when out of memory.
int   gmap_init(struct gmap* m, unsigned int key_size, unsigned int value_size, unsigned int initial_cap,
               unsigned int (*hash)(const void* key, unsigned int key_size), int (*eq)(const void* a, const void* b));
void  gmap_free(struct gmap* m);                                       // frees the table; the values are the caller's, untouched
void  gmap_clear(struct gmap* m);                                      // removes every entry, keeps the table
int   gmap_set(struct gmap* m, const void* key, const void* value);    // adds or replaces; 0 ok, -1 when out of memory
void* gmap_get(const struct gmap* m, const void* key);                 // pointer to the stored value, or NULL
int   gmap_has(const struct gmap* m, const void* key);
int   gmap_remove(struct gmap* m, const void* key);                    // 1 when it was there, 0 when not
void  gmap_each(const struct gmap* m, void (*fn)(const void* key, void* value, void* ctx), void* ctx);   // in no particular order

static inline unsigned int gmap_len(const struct gmap* m) { return m->len; }
```
