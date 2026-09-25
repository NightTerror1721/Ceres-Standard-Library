# `<ceres/ds/rbtree.h>`

A red-black tree whose node lives INSIDE the object it orders: no allocation, no copy, the same idea as ceres/ds/list.h taken to a tree - the style Linux's own rbtree.h uses. O(log n) guaranteed worst case for insert, remove and find, which neither ceres/ds/iheap.h (a heap only guarantees the minimum, not arbitrary find) nor a plain sorted vector (O(n) insert) gives.

```c
struct job { int priority; struct rb_node node; };
int by_priority(const struct rb_node* a, const struct rb_node* b) {
    return rb_entry(a, struct job, node)->priority - rb_entry(b, struct job, node)->priority;
}
struct rbtree t;  rb_init(&t, by_priority);
struct job j = { 3 };  rb_insert(&t, &j.node);
struct rb_node* it;
for (it = rb_first(&t); it != NULL; it = rb_next(it))
    ... rb_entry(it, struct job, node) ...
```

rb_insert does not check for a duplicate key - if the tree may already hold one, rb_find it first and decide what a duplicate means for that tree (replace it, keep both, reject the insert). The tree does not own what it orders: rb_remove only unlinks a node, it never frees anything.

```c
struct rb_node
{
    struct rb_node* parent;
    struct rb_node* left;
    struct rb_node* right;
    int red;             // 1 red, 0 black - never read by anything outside rbtree.c
};

struct rbtree
{
    struct rb_node* root;    // NULL when empty
    unsigned int count;
    int (*cmp)(const struct rb_node* a, const struct rb_node* b);
};

void rb_init(struct rbtree* t, int (*cmp)(const struct rb_node* a, const struct rb_node* b));
void rb_insert(struct rbtree* t, struct rb_node* n);                          // caller-decided duplicate policy - see above
void rb_remove(struct rbtree* t, struct rb_node* n);                          // n must currently be in t
struct rb_node* rb_find(const struct rbtree* t, const struct rb_node* key);   // NULL when no match

// In-order traversal: the smallest node, then each node's successor, NULL after the largest.
struct rb_node* rb_first(const struct rbtree* t);
struct rb_node* rb_last(const struct rbtree* t);
struct rb_node* rb_next(const struct rb_node* n);
struct rb_node* rb_prev(const struct rb_node* n);

static inline unsigned int rb_count(const struct rbtree* t) { return t->count; }
static inline int rb_empty(const struct rbtree* t) { return t->root == NULL; }

// The object a node is embedded in - the same trick as list.h's list_entry.
#define rb_entry(node, type, member) ((type*)((char*)(node) - offsetof(type, member)))
```
