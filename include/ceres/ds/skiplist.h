#pragma once

#include "../rand.h"

// A skip list: an ordered map by coin flips instead of rotations, documented next to
// ceres/ds/rbtree.h as the alternative to reach for if a rotation/color bug ever turns up there -
// same O(log n) expected cost, far less code to have gotten wrong. Unlike rbtree.h (intrusive) and
// omap.h (copies), a skl_node holds POINTERS to the caller's own key and value - it neither copies
// them nor demands a field inside them, at the cost of one allocation per entry.
//
//   struct skiplist s;  skl_init(&s, cmp_cstr);
//   char* k = "b";  int v = 2;
//   skl_insert(&s, k, &v);
//   int* found = (int*)skl_find(&s, "b");
//
// cmp compares two KEYS - pqueue.h's convention. skl_insert does not check for a duplicate key,
// the same policy as rb_insert; skl_find only ever returns the first match a search would reach.

#define SKL_MAX_LEVEL 16   // p = 1/2 per level: room for well over 60,000 entries before balance degrades

struct skl_node
{
    void* key;
    void* value;
    unsigned int height;        // levels this node actually participates in: 1 .. SKL_MAX_LEVEL
    struct skl_node* next[1];   // flexible array, really `height` entries - see offsetof in skiplist.c
};

struct skiplist
{
    struct skl_node* head;      // a dummy of height SKL_MAX_LEVEL; head->key/value are never read
    unsigned int count;
    unsigned int level;         // the highest level currently in use: 1 .. SKL_MAX_LEVEL
    int (*cmp)(const void* a, const void* b);
    struct rng rng;             // this skip list's own coin - see ceres/rand.h
};

int   skl_init(struct skiplist* s, int (*cmp)(const void* a, const void* b));   // 0 ok, -1 out of memory
void  skl_free(struct skiplist* s);
int   skl_insert(struct skiplist* s, void* key, void* value);                    // 0 ok, -1 out of memory
void* skl_find(const struct skiplist* s, const void* key);                       // NULL when no match
int   skl_remove(struct skiplist* s, const void* key);                           // 1 removed, 0 was not there

static inline unsigned int skl_len(const struct skiplist* s) { return s->count; }

// In-order walk, smallest first: skl_first, then skl_next from the node it returned, until NULL.
static inline struct skl_node* skl_first(const struct skiplist* s) { return s->head->next[0]; }
static inline struct skl_node* skl_next(const struct skl_node* n) { return n->next[0]; }
