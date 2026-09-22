// A growable circular buffer. See ceres/ds/deque.h.
#include "ceres/ds/deque.h"
#include "stdlib.h"
#include "string.h"

void dq_init(struct deque* d, unsigned int elem_size)
{
    d->data = NULL;
    d->head = 0;
    d->len = 0;
    d->cap = 0;
    d->elem = elem_size == 0 ? 1u : elem_size;
}

void dq_free(struct deque* d)
{
    free(d->data);
    d->data = NULL;
    d->head = 0;
    d->len = 0;
    d->cap = 0;
}

void dq_clear(struct deque* d)
{
    d->head = 0;
    d->len = 0;
}

// Slot i (logical index, 0 == the front) as a byte offset into data.
static unsigned int slot_of(const struct deque* d, unsigned int i)
{
    unsigned int pos = d->head + i;
    if (pos >= d->cap)
        pos -= d->cap;
    return pos;
}

static int grow(struct deque* d)
{
    unsigned int want = d->cap == 0 ? 4u : d->cap * 2u;
    if (want < d->cap)                                  // doubled past 2^32
        return -1;
    if (want > 0xFFFFFFFFu / d->elem)
        return -1;
    void* bigger = malloc((size_t)want * d->elem);
    if (bigger == NULL)
        return -1;
    // Copy out in logical order, unwrapping the circle, so the new buffer starts at head == 0 -
    // simpler than growing in place and re-deriving where the wrap now falls.
    for (unsigned int i = 0; i < d->len; i++)
        memcpy((char*)bigger + (size_t)i * d->elem, (char*)d->data + (size_t)slot_of(d, i) * d->elem, d->elem);
    free(d->data);
    d->data = bigger;
    d->cap = want;
    d->head = 0;
    return 0;
}

int dq_push_back(struct deque* d, const void* item)
{
    if (d->len == d->cap && grow(d) != 0)
        return -1;
    unsigned int at = slot_of(d, d->len);
    memcpy((char*)d->data + (size_t)at * d->elem, item, d->elem);
    d->len++;
    return 0;
}

int dq_push_front(struct deque* d, const void* item)
{
    if (d->len == d->cap && grow(d) != 0)
        return -1;
    d->head = d->head == 0 ? d->cap - 1u : d->head - 1u;
    memcpy((char*)d->data + (size_t)d->head * d->elem, item, d->elem);
    d->len++;
    return 0;
}

int dq_pop_back(struct deque* d, void* out)
{
    if (d->len == 0)
        return -1;
    unsigned int at = slot_of(d, d->len - 1u);
    if (out != NULL)
        memcpy(out, (char*)d->data + (size_t)at * d->elem, d->elem);
    d->len--;
    return 0;
}

int dq_pop_front(struct deque* d, void* out)
{
    if (d->len == 0)
        return -1;
    if (out != NULL)
        memcpy(out, (char*)d->data + (size_t)d->head * d->elem, d->elem);
    d->head = slot_of(d, 1);
    d->len--;
    return 0;
}

void* dq_at(const struct deque* d, unsigned int i)
{
    if (i >= d->len)
        return NULL;
    return (char*)d->data + (size_t)slot_of(d, i) * d->elem;
}
