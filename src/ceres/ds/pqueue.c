// Binary heap. See ceres/ds/pqueue.h.
#include "ceres/ds/pqueue.h"
#include "string.h"
#include "heap_priv.h"

void pq_init(struct pqueue* q, unsigned int elem_size, int (*cmp)(const void*, const void*))
{
    vector_init(&q->items, elem_size);
    q->cmp = cmp;
}

void pq_free(struct pqueue* q)
{
    vector_free(&q->items);
}

void pq_clear(struct pqueue* q)
{
    vector_clear(&q->items);
}

const void* pq_peek(const struct pqueue* q)
{
    return vector_at(&q->items, 0);
}

static void swap_elems(struct pqueue* q, unsigned int i, unsigned int j)
{
    unsigned char* a = (unsigned char*)vector_at(&q->items, i);
    unsigned char* b = (unsigned char*)vector_at(&q->items, j);
    for (unsigned int k = 0; k < q->items.elem; k++)
    {
        unsigned char t = a[k];
        a[k] = b[k];
        b[k] = t;
    }
}

#define PQ_LESS(i, j) (q->cmp(vector_at(&q->items, (i)), vector_at(&q->items, (j))))
#define PQ_SWAP(i, j) swap_elems(q, (i), (j))

static void sift_up(struct pqueue* q, unsigned int i)   { HEAP_SIFT_UP(i, PQ_LESS, PQ_SWAP); }
static void sift_down(struct pqueue* q, unsigned int i) { HEAP_SIFT_DOWN(i, q->items.len, PQ_LESS, PQ_SWAP); }

#undef PQ_LESS
#undef PQ_SWAP

int pq_push(struct pqueue* q, const void* item)
{
    if (vector_push_copy(&q->items, item) != 0)
        return -1;
    sift_up(q, q->items.len - 1u);
    return 0;
}

int pq_pop(struct pqueue* q, void* out)
{
    if (q->items.len == 0)
        return -1;
    if (out != NULL)
        memcpy(out, vector_at(&q->items, 0), q->items.elem);
    unsigned int last = q->items.len - 1u;
    if (last > 0)
        swap_elems(q, 0, last);
    vector_pop(&q->items);
    if (last > 0)
        sift_down(q, 0);
    return 0;
}
