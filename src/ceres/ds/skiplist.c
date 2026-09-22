// A skip list. See ceres/ds/skiplist.h.
#include "ceres/ds/skiplist.h"
#include "stdlib.h"

static struct skl_node* alloc_node(unsigned int height, void* key, void* value)
{
    struct skl_node* n = (struct skl_node*)malloc(offsetof(struct skl_node, next) + (size_t)height * sizeof(struct skl_node*));
    if (n == NULL)
        return NULL;
    n->key = key;
    n->value = value;
    n->height = height;
    for (unsigned int i = 0; i < height; i++)
        n->next[i] = NULL;
    return n;
}

int skl_init(struct skiplist* s, int (*cmp)(const void*, const void*))
{
    s->head = alloc_node(SKL_MAX_LEVEL, NULL, NULL);
    if (s->head == NULL)
        return -1;
    s->count = 0;
    s->level = 1;
    s->cmp = cmp;
    rng_seed(&s->rng, 0x5eed5eedu);          // deterministic by default - only balance depends on it, never correctness
    return 0;
}

void skl_free(struct skiplist* s)
{
    struct skl_node* x = s->head->next[0];
    while (x != NULL)
    {
        struct skl_node* next = x->next[0];   // the bottom level links every node, so this alone visits them all
        free(x);
        x = next;
    }
    free(s->head);
    s->head = NULL;
    s->count = 0;
    s->level = 0;
}

// Coin flips until the first tail, capped at SKL_MAX_LEVEL - the standard construction.
static unsigned int random_height(struct skiplist* s)
{
    unsigned int h = 1;
    while (h < SKL_MAX_LEVEL && rng_chance(&s->rng, 50))
        h++;
    return h;
}

int skl_insert(struct skiplist* s, void* key, void* value)
{
    struct skl_node* update[SKL_MAX_LEVEL];
    struct skl_node* x = s->head;
    for (int i = (int)s->level - 1; i >= 0; i--)
    {
        while (x->next[i] != NULL && s->cmp(x->next[i]->key, key) < 0)
            x = x->next[i];
        update[i] = x;
    }

    unsigned int height = random_height(s);
    if (height > s->level)
    {
        for (unsigned int i = s->level; i < height; i++)
            update[i] = s->head;
        s->level = height;
    }

    struct skl_node* node = alloc_node(height, key, value);
    if (node == NULL)
        return -1;
    for (unsigned int i = 0; i < height; i++)
    {
        node->next[i] = update[i]->next[i];
        update[i]->next[i] = node;
    }
    s->count++;
    return 0;
}

void* skl_find(const struct skiplist* s, const void* key)
{
    struct skl_node* x = s->head;
    for (int i = (int)s->level - 1; i >= 0; i--)
    {
        while (x->next[i] != NULL && s->cmp(x->next[i]->key, key) < 0)
            x = x->next[i];
    }
    struct skl_node* candidate = x->next[0];
    if (candidate != NULL && s->cmp(candidate->key, key) == 0)
        return candidate->value;
    return NULL;
}

int skl_remove(struct skiplist* s, const void* key)
{
    struct skl_node* update[SKL_MAX_LEVEL];
    struct skl_node* x = s->head;
    for (int i = (int)s->level - 1; i >= 0; i--)
    {
        while (x->next[i] != NULL && s->cmp(x->next[i]->key, key) < 0)
            x = x->next[i];
        update[i] = x;
    }
    struct skl_node* target = x->next[0];
    if (target == NULL || s->cmp(target->key, key) != 0)
        return 0;

    for (unsigned int i = 0; i < s->level; i++)
    {
        if (update[i]->next[i] != target)
            break;                            // target does not reach this level: neither do the levels above it
        update[i]->next[i] = target->next[i];
    }
    free(target);
    while (s->level > 1 && s->head->next[s->level - 1] == NULL)
        s->level--;
    s->count--;
    return 1;
}
