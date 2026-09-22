// A rbtree.h of small allocations. See ceres/ds/omap.h.
#include "ceres/ds/omap.h"
#include "stdlib.h"
#include "string.h"

struct omap_node
{
    struct rb_node rb;                 // first field: rb_entry(&n->rb, struct omap_node, rb) == n, trivially
    unsigned char data[1];             // key_size bytes of key, then value_size bytes of value
};

// rbtree.h's comparator takes no context parameter, so the one comparator this file ever registers
// has to reach `m->cmp` some other way: set immediately before any rbtree call that might invoke
// it, read back inside it. Safe because nothing here is reentrant and this library assumes a
// single thread throughout - one omap call finishes, on this or any other omap, before the next
// begins.
static int (*g_active_cmp)(const void*, const void*);

static int node_cmp(const struct rb_node* a, const struct rb_node* b)
{
    const struct omap_node* na = rb_entry(a, struct omap_node, rb);
    const struct omap_node* nb = rb_entry(b, struct omap_node, rb);
    return g_active_cmp(na->data, nb->data);
}

void omap_init(struct omap* m, unsigned int key_size, unsigned int value_size, int (*cmp)(const void*, const void*))
{
    rb_init(&m->tree, node_cmp);
    m->key_size = key_size;
    m->value_size = value_size;
    m->cmp = cmp;
}

void omap_free(struct omap* m)
{
    struct rb_node* it = rb_first(&m->tree);
    while (it != NULL)
    {
        struct rb_node* next = rb_next(it);          // computed before freeing `it` - see list.h's
                                                        // LIST_FOR_EACH_SAFE for the same idea one
                                                        // structure over; an ancestor `it` needs for
                                                        // this very call is never freed before it
        free(rb_entry(it, struct omap_node, rb));
        it = next;
    }
    rb_init(&m->tree, m->tree.cmp);
}

// A throwaway node built only to search by key - never inserted, never read back through rb_entry
// by anything but node_cmp. malloc'd rather than stack-allocated because key_size is a run-time
// value: a fixed-size stack buffer would cap how wide a key omap could hold.
static struct omap_node* find_node(const struct omap* m, const void* key)
{
    struct omap_node* probe = (struct omap_node*)malloc(offsetof(struct omap_node, data) + m->key_size);
    if (probe == NULL)
        return NULL;                                  // a lookup this starved reports "not found"
    memcpy(probe->data, key, m->key_size);
    g_active_cmp = m->cmp;
    struct rb_node* found = rb_find(&m->tree, &probe->rb);
    free(probe);
    return found == NULL ? NULL : rb_entry(found, struct omap_node, rb);
}

int omap_set(struct omap* m, const void* key, const void* value)
{
    struct omap_node* existing = find_node(m, key);
    if (existing != NULL)
    {
        if (m->value_size)
            memcpy(existing->data + m->key_size, value, m->value_size);
        return 0;
    }

    struct omap_node* node = (struct omap_node*)malloc(offsetof(struct omap_node, data) + m->key_size + m->value_size);
    if (node == NULL)
        return -1;
    memcpy(node->data, key, m->key_size);
    if (m->value_size)
        memcpy(node->data + m->key_size, value, m->value_size);
    g_active_cmp = m->cmp;
    rb_insert(&m->tree, &node->rb);
    return 0;
}

void* omap_get(const struct omap* m, const void* key)
{
    struct omap_node* n = find_node(m, key);
    return n == NULL ? NULL : n->data + m->key_size;
}

int omap_has(const struct omap* m, const void* key)
{
    struct omap_node* n = find_node(m, key);
    return n != NULL;
}

int omap_remove(struct omap* m, const void* key)
{
    struct omap_node* n = find_node(m, key);
    if (n == NULL)
        return 0;
    rb_remove(&m->tree, &n->rb);
    free(n);
    return 1;
}

const void* omap_first_key(const struct omap* m)
{
    struct rb_node* first = rb_first(&m->tree);
    return first == NULL ? NULL : rb_entry(first, struct omap_node, rb)->data;
}

const void* omap_next_key(const struct omap* m, const void* key)
{
    (void)m;
    const struct omap_node* node = (const struct omap_node*)((const char*)key - offsetof(struct omap_node, data));
    struct rb_node* next = rb_next(&node->rb);
    return next == NULL ? NULL : rb_entry(next, struct omap_node, rb)->data;
}
