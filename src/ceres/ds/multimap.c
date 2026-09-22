// A gmap of chains. See ceres/ds/multimap.h.
#include "ceres/ds/multimap.h"
#include "stdlib.h"
#include "string.h"

int mm_init(struct multimap* m, unsigned int key_size, unsigned int value_size, unsigned int initial_cap,
    unsigned int (*hash)(const void*, unsigned int), int (*eq)(const void*, const void*))
{
    m->value_size = value_size;
    return gmap_init(&m->keys, key_size, sizeof(struct mm_chain*), initial_cap, hash, eq);
}

static void free_chain(struct mm_chain* node)
{
    while (node != NULL)
    {
        struct mm_chain* next = node->next;
        free(node);
        node = next;
    }
}

static void free_chain_cb(const void* key, void* value, void* ctx)
{
    (void)key;
    (void)ctx;
    free_chain(*(struct mm_chain**)value);
}

void mm_free(struct multimap* m)
{
    gmap_each(&m->keys, free_chain_cb, NULL);
    gmap_free(&m->keys);
}

void mm_clear(struct multimap* m)
{
    gmap_each(&m->keys, free_chain_cb, NULL);
    gmap_clear(&m->keys);
}

int mm_add(struct multimap* m, const void* key, const void* value)
{
    struct mm_chain* node = (struct mm_chain*)malloc(offsetof(struct mm_chain, value) + m->value_size);
    if (node == NULL)
        return -1;
    memcpy(node->value, value, m->value_size);

    struct mm_chain** head = (struct mm_chain**)gmap_get(&m->keys, key);
    if (head != NULL)
    {
        node->next = *head;
        *head = node;
        return 0;
    }

    node->next = NULL;
    if (gmap_set(&m->keys, key, &node) != 0)
    {
        free(node);
        return -1;
    }
    return 0;
}

int mm_remove(struct multimap* m, const void* key, const void* value)
{
    struct mm_chain** head = (struct mm_chain**)gmap_get(&m->keys, key);
    if (head == NULL)
        return 0;

    struct mm_chain* prev = NULL;
    struct mm_chain* node = *head;
    while (node != NULL)
    {
        if (memcmp(node->value, value, m->value_size) == 0)
        {
            if (prev == NULL)
                *head = node->next;
            else
                prev->next = node->next;
            free(node);
            if (*head == NULL)
                gmap_remove(&m->keys, key);         // an empty chain is the same as no entry
            return 1;
        }
        prev = node;
        node = node->next;
    }
    return 0;
}

void mm_each(const struct multimap* m, const void* key, void (*fn)(const void* value, void* ctx), void* ctx)
{
    struct mm_chain** head = (struct mm_chain**)gmap_get(&m->keys, key);
    if (head == NULL)
        return;
    for (struct mm_chain* node = *head; node != NULL; node = node->next)
        fn(node->value, ctx);
}

unsigned int mm_count(const struct multimap* m, const void* key)
{
    struct mm_chain** head = (struct mm_chain**)gmap_get(&m->keys, key);
    if (head == NULL)
        return 0;
    unsigned int n = 0;
    for (struct mm_chain* node = *head; node != NULL; node = node->next)
        n++;
    return n;
}
