// A gmap indexing a recency-ordered list. See ceres/ds/lru.h.
#include "ceres/ds/lru.h"
#include "stdlib.h"
#include "string.h"

int lru_init(struct lru* c, unsigned int key_size, unsigned int value_size, unsigned int capacity,
    unsigned int (*hash)(const void*, unsigned int), int (*eq)(const void*, const void*))
{
    c->capacity = capacity;
    c->key_size = key_size;
    c->value_size = value_size;
    list_init(&c->order);
    return gmap_init(&c->index, key_size, sizeof(struct lru_entry*), 0, hash, eq);
}

void lru_free(struct lru* c)
{
    struct list_node* it;
    struct list_node* nx;
    LIST_FOR_EACH_SAFE(it, nx, &c->order)
    {
        list_remove(&c->order, it);
        free(list_entry(it, struct lru_entry, link));
    }
    gmap_free(&c->index);
}

static void touch(struct lru* c, struct lru_entry* e)
{
    list_remove(&c->order, &e->link);
    list_push_front(&c->order, &e->link);
}

int lru_put(struct lru* c, const void* key, const void* value)
{
    struct lru_entry** slot = (struct lru_entry**)gmap_get(&c->index, key);
    if (slot != NULL)
    {
        memcpy((*slot)->data + c->key_size, value, c->value_size);
        touch(c, *slot);
        return 0;
    }

    struct lru_entry* e = (struct lru_entry*)malloc(offsetof(struct lru_entry, data) + c->key_size + c->value_size);
    if (e == NULL)
        return -1;
    list_node_init(&e->link);
    memcpy(e->data, key, c->key_size);
    memcpy(e->data + c->key_size, value, c->value_size);

    if (gmap_set(&c->index, key, &e) != 0)
    {
        free(e);
        return -1;
    }
    list_push_front(&c->order, &e->link);

    if (c->capacity > 0 && gmap_len(&c->index) > c->capacity)
    {
        struct list_node* oldest = list_last(&c->order);
        struct lru_entry* victim = list_entry(oldest, struct lru_entry, link);
        gmap_remove(&c->index, victim->data);
        list_remove(&c->order, oldest);
        free(victim);
    }
    return 0;
}

void* lru_get(struct lru* c, const void* key)
{
    struct lru_entry** slot = (struct lru_entry**)gmap_get(&c->index, key);
    if (slot == NULL)
        return NULL;
    touch(c, *slot);
    return (*slot)->data + c->key_size;
}

int lru_has(const struct lru* c, const void* key)
{
    return gmap_has(&c->index, key);
}

int lru_remove(struct lru* c, const void* key)
{
    struct lru_entry** slot = (struct lru_entry**)gmap_get(&c->index, key);
    if (slot == NULL)
        return 0;
    struct lru_entry* e = *slot;
    list_remove(&c->order, &e->link);
    gmap_remove(&c->index, key);
    free(e);
    return 1;
}
