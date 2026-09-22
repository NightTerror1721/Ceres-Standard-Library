// A fixed-capacity circular queue of elements. See ceres/ds/cqueue.h.
#include "ceres/ds/cqueue.h"
#include "ceres/bits.h"
#include "string.h"

void cq_init(struct cqueue* q, void* storage, unsigned int elem_size, unsigned int count)
{
    unsigned int pow2 = 1;
    if (count > 1)
        pow2 = 1u << bit_log2(count);                       // the largest power of two that fits
    q->buf = (unsigned char*)storage;
    q->elem = elem_size == 0 ? 1u : elem_size;
    q->mask = pow2 - 1u;
    q->head = 0;
    q->tail = 0;
}

unsigned int cq_count(const struct cqueue* q)
{
    return q->head - q->tail;                               // wraps correctly: both run freely
}

unsigned int cq_space(const struct cqueue* q)
{
    return q->mask + 1u - cq_count(q);
}

int cq_put(struct cqueue* q, const void* item)
{
    unsigned int head = q->head;
    if (head - q->tail > q->mask)                           // capacity elements waiting: full
        return -1;
    memcpy(q->buf + (size_t)(head & q->mask) * q->elem, item, q->elem);
    q->head = head + 1u;                                    // published only after the element is in place
    return 0;
}

int cq_get(struct cqueue* q, void* out)
{
    unsigned int tail = q->tail;
    if (q->head == tail)
        return -1;
    if (out != NULL)
        memcpy(out, q->buf + (size_t)(tail & q->mask) * q->elem, q->elem);
    q->tail = tail + 1u;
    return 0;
}

void cq_clear(struct cqueue* q)
{
    q->tail = q->head;
}
