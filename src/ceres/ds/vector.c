// Growable array. See ceres/ds/vector.h.
#include "ceres/ds/vector.h"
#include "ceres/sort.h"
#include "stdlib.h"
#include "string.h"

void vector_init(struct vector* v, unsigned int elem_size)
{
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
    v->elem = elem_size == 0 ? 1u : elem_size;
}

void vector_free(struct vector* v)
{
    free(v->data);
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

int vector_reserve(struct vector* v, unsigned int cap)
{
    if (cap <= v->cap)
        return 0;
    if (cap > 0xFFFFFFFFu / v->elem)                    // the byte count would not fit
        return -1;
    void* bigger = realloc(v->data, (size_t)cap * v->elem);
    if (bigger == NULL)
        return -1;
    v->data = bigger;
    v->cap = cap;
    return 0;
}

static int make_room_for_one(struct vector* v)
{
    if (v->len < v->cap)
        return 0;
    unsigned int want = v->cap == 0 ? 4u : v->cap * 2u;
    if (want < v->cap)                                  // doubled past 2^32
        return -1;
    return vector_reserve(v, want);
}

void* vector_push(struct vector* v)
{
    if (make_room_for_one(v) != 0)
        return NULL;
    void* slot = (char*)v->data + (size_t)v->len * v->elem;
    memset(slot, 0, v->elem);
    v->len++;
    return slot;
}

int vector_push_copy(struct vector* v, const void* item)
{
    if (make_room_for_one(v) != 0)
        return -1;
    memcpy((char*)v->data + (size_t)v->len * v->elem, item, v->elem);
    v->len++;
    return 0;
}

void* vector_at(const struct vector* v, unsigned int i)
{
    if (i >= v->len)
        return NULL;
    return (char*)v->data + (size_t)i * v->elem;
}

void* vector_last(const struct vector* v)
{
    return v->len == 0 ? NULL : vector_at(v, v->len - 1);
}

void vector_pop(struct vector* v)
{
    if (v->len > 0)
        v->len--;
}

int vector_insert(struct vector* v, unsigned int i, const void* item)
{
    if (i > v->len)
        return -1;
    if (make_room_for_one(v) != 0)
        return -1;
    char* at = (char*)v->data + (size_t)i * v->elem;
    memmove(at + v->elem, at, (size_t)(v->len - i) * v->elem);
    memcpy(at, item, v->elem);
    v->len++;
    return 0;
}

void vector_remove(struct vector* v, unsigned int i)
{
    if (i >= v->len)
        return;
    char* at = (char*)v->data + (size_t)i * v->elem;
    memmove(at, at + v->elem, (size_t)(v->len - i - 1) * v->elem);
    v->len--;
}

void vector_clear(struct vector* v)
{
    v->len = 0;
}

void vector_sort(struct vector* v, int (*cmp)(const void*, const void*))
{
    if (v->len > 1)
        qsort(v->data, v->len, v->elem, cmp);
}

void vector_sort_r(struct vector* v, int (*cmp)(const void*, const void*, void*), void* ctx)
{
    if (v->len > 1)
        qsort_r(v->data, v->len, v->elem, cmp, ctx);
}

int vector_sort_stable(struct vector* v, int (*cmp)(const void*, const void*))
{
    return sort_stable(v->data, v->len, v->elem, cmp);
}
