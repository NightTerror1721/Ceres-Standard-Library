// A binary heap with a stable handle per element. See ceres/ds/iheap.h.
#include "ceres/ds/iheap.h"
#include "string.h"

void ih_init(struct iheap* h, unsigned int elem_size, int (*cmp)(const void*, const void*))
{
    vector_init(&h->items, elem_size);
    vector_init(&h->heap, sizeof(unsigned int));
    vector_init(&h->pos, sizeof(unsigned int));
    h->cmp = cmp;
}

void ih_free(struct iheap* h)
{
    vector_free(&h->items);
    vector_free(&h->heap);
    vector_free(&h->pos);
}

unsigned int ih_len(const struct iheap* h) { return h->heap.len; }

const void* ih_peek(const struct iheap* h)
{
    if (h->heap.len == 0)
        return NULL;
    unsigned int handle = *(unsigned int*)vector_at(&h->heap, 0);
    return vector_at(&h->items, handle);
}

static void* item_of(struct iheap* h, unsigned int heap_pos)
{
    unsigned int handle = *(unsigned int*)vector_at(&h->heap, heap_pos);
    return vector_at(&h->items, handle);
}

// Swaps two heap slots and keeps `pos` pointing at where each handle actually ended up.
static void swap_heap(struct iheap* h, unsigned int i, unsigned int j)
{
    unsigned int* a = (unsigned int*)vector_at(&h->heap, i);
    unsigned int* b = (unsigned int*)vector_at(&h->heap, j);
    unsigned int t = *a;
    *a = *b;
    *b = t;
    *(unsigned int*)vector_at(&h->pos, *a) = i;
    *(unsigned int*)vector_at(&h->pos, *b) = j;
}

static void sift_up(struct iheap* h, unsigned int i)
{
    while (i > 0)
    {
        unsigned int parent = (i - 1u) / 2u;
        if (h->cmp(item_of(h, i), item_of(h, parent)) >= 0)
            break;
        swap_heap(h, i, parent);
        i = parent;
    }
}

static void sift_down(struct iheap* h, unsigned int i)
{
    unsigned int n = h->heap.len;
    for (;;)
    {
        unsigned int left = 2u * i + 1u;
        unsigned int right = left + 1u;
        unsigned int best = i;
        if (left < n && h->cmp(item_of(h, left), item_of(h, best)) < 0)
            best = left;
        if (right < n && h->cmp(item_of(h, right), item_of(h, best)) < 0)
            best = right;
        if (best == i)
            return;
        swap_heap(h, i, best);
        i = best;
    }
}

unsigned int ih_push(struct iheap* h, const void* item)
{
    unsigned int handle = h->items.len;
    if (vector_push_copy(&h->items, item) != 0)
        return IH_NOT_IN_HEAP;
    unsigned int heap_pos = h->heap.len;
    if (vector_push_copy(&h->heap, &handle) != 0)
        return IH_NOT_IN_HEAP;             // items and pos stay one entry ahead; harmless, never reached again
    if (vector_push_copy(&h->pos, &heap_pos) != 0)
        return IH_NOT_IN_HEAP;
    sift_up(h, heap_pos);
    return handle;
}

int ih_pop(struct iheap* h, void* out)
{
    if (h->heap.len == 0)
        return -1;
    unsigned int top_handle = *(unsigned int*)vector_at(&h->heap, 0);
    if (out != NULL)
        memcpy(out, vector_at(&h->items, top_handle), h->items.elem);
    unsigned int last = h->heap.len - 1u;
    if (last > 0)
        swap_heap(h, 0, last);
    vector_pop(&h->heap);
    *(unsigned int*)vector_at(&h->pos, top_handle) = IH_NOT_IN_HEAP;
    if (last > 0)
        sift_down(h, 0);
    return 0;
}

void ih_decrease(struct iheap* h, unsigned int handle, const void* new_item)
{
    if (handle >= h->pos.len)
        return;
    unsigned int p = *(unsigned int*)vector_at(&h->pos, handle);
    if (p == IH_NOT_IN_HEAP)
        return;
    memcpy(vector_at(&h->items, handle), new_item, h->items.elem);
    // Sifting both ways makes this correct whether the new item sorts earlier or later, not only
    // on a genuine decrease - still O(log n), and it means one function covers both directions.
    sift_up(h, p);
    p = *(unsigned int*)vector_at(&h->pos, handle);     // sift_up may have moved it
    sift_down(h, p);
}

void ih_remove(struct iheap* h, unsigned int handle)
{
    if (handle >= h->pos.len)
        return;
    unsigned int p = *(unsigned int*)vector_at(&h->pos, handle);
    if (p == IH_NOT_IN_HEAP)
        return;
    unsigned int last = h->heap.len - 1u;
    if (p != last)
    {
        swap_heap(h, p, last);
        vector_pop(&h->heap);
        // Whatever now sits at p (previously at `last`) may be out of place in either direction.
        sift_up(h, p);
        sift_down(h, p);
    }
    else
    {
        vector_pop(&h->heap);
    }
    *(unsigned int*)vector_at(&h->pos, handle) = IH_NOT_IN_HEAP;
}
