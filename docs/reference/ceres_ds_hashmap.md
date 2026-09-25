# `<ceres/ds/hashmap.h>`

A hash map from strings to pointers: open addressing with linear probing, growing before it gets crowded. The map keeps its own copy of every key; the values are the caller's pointers and are never followed or freed. Removed entries leave a marker so a probe chain is not cut short.

```c
struct hashmap_slot
{
    char* key;                 // NULL: never used; the tombstone marker: removed; otherwise a copy owned by the map
    void* value;
    unsigned int hash;
};

struct hashmap
{
    struct hashmap_slot* slots;
    unsigned int cap;          // a power of two
    unsigned int len;          // entries stored
    unsigned int used;         // slots that are not empty: entries plus tombstones
};

int   hashmap_init(struct hashmap* m, unsigned int initial_cap);      // 0 ok, -1 when out of memory (cap is rounded up to a power of two, at least 8)
void  hashmap_free(struct hashmap* m);                                 // frees the keys and the table, not the values
int   hashmap_set(struct hashmap* m, const char* key, void* value);    // adds or replaces; 0 ok, -1 when out of memory
void* hashmap_get(const struct hashmap* m, const char* key);           // the value, or NULL when there is no such key
int   hashmap_has(const struct hashmap* m, const char* key);           // 1 when the key is present, even with a NULL value
int   hashmap_remove(struct hashmap* m, const char* key);              // 1 when it was there, 0 when not
void  hashmap_clear(struct hashmap* m);                                // removes every entry, keeps the table
void  hashmap_each(const struct hashmap* m, void (*fn)(const char* key, void* value, void* ctx), void* ctx);   // in no particular order

static inline unsigned int hashmap_len(const struct hashmap* m) { return m->len; }
```
