# `<ceres/ds/trie.h>`

A trie over ASCII strings: "every word starting with this prefix" is a query neither ceres/ds/hashmap.h nor ceres/ds/omap.h answers well - the tree would have to compare whole keys at every node, character by character is what a trie is for. Nodes come from a ceres/pool.h the caller sizes up front (one node per distinct character position across every key inserted, at most - see trie_init), not malloc: the one place among the eighteen collections where a fixed capacity is actually the natural shape, since pool.h's own constraint (the block count has to be known at init) is no different from what a trie already needs the caller to have thought about. A node's children are a ceres/ds/slist.h kept sorted by character, not a 256-entry array - most alphabets in practice are sparse at any one branching point.

```c
char storage[64 * sizeof(struct trie_node)];   // room for 64 nodes
struct trie t;  trie_init(&t, storage, 64);
trie_insert(&t, "cat", &cat_data);
trie_insert(&t, "car", &car_data);
void* found = trie_find(&t, "cat");
trie_each_prefix(&t, "ca", print_word, NULL);   // visits "car" then "cat", in that order
```

The trie never owns what `value` points at - inserting, finding and walking only ever hand that pointer back, never follow or free it.

```c
struct trie_node
{
    char ch;
    void* value;                    // meaningful only when is_word
    int is_word;
    struct slist_node siblings;     // links this node into its parent's `children`
    struct slist children;          // this node's own children, kept sorted by ch
};

struct trie
{
    struct trie_node root;          // root.ch/is_word/value are never meaningful - root.children is the first level
    struct pool nodes;
};

// `node_storage` needs sizeof(struct trie_node) * max_nodes bytes - one node per distinct
// (position, character) pair across every key ever inserted, so a rough upper bound is the sum of
// every key's length, or just the total character count of the dictionary if it is known up front.
void  trie_init(struct trie* t, void* node_storage, unsigned int max_nodes);
void  trie_clear(struct trie* t);                                         // every node freed back to the pool, root has no children again
int   trie_insert(struct trie* t, const char* key, void* value);          // 0 ok, -1 the pool is full
void* trie_find(const struct trie* t, const char* key);                   // NULL when key was never inserted (or was, but as a prefix only)
int   trie_has_prefix(const struct trie* t, const char* prefix);          // 1 when some inserted key starts with prefix (prefix itself included)

// Calls fn(word, value, ctx) for every inserted key that starts with `prefix` ("" matches
// everything), each word built into an internal buffer capped at 255 characters - long past
// anything this is for, and failing safe (that branch is simply not descended) rather than
// overflowing if some key ever is longer. Visits words in ascending order at every branching point,
// because children are kept sorted; not overall alphabetical order across different lengths.
void trie_each_prefix(const struct trie* t, const char* prefix, void (*fn)(const char* word, void* value, void* ctx), void* ctx);
```
