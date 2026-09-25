# `<ceres/ds/pqueue.h>`

A priority queue: a binary min-heap of fixed-size elements ordered by a comparison function. Push and pop cost O(log n). Used for shortest-path search, timers and schedulers.

cmp(a, b) < 0 means `a` leaves the queue before `b`. For a max-queue return the opposite sign.

Elements are copied in and out; the queue is built on ceres/ds/vector.h.

```c
struct pqueue
{
    struct vector items;
    int (*cmp)(const void*, const void*);
};

void  pq_init(struct pqueue* q, unsigned int elem_size, int (*cmp)(const void*, const void*));
void  pq_free(struct pqueue* q);
void  pq_clear(struct pqueue* q);                       // empties it, keeps the storage
int   pq_push(struct pqueue* q, const void* item);      // 0 ok, -1 when out of memory
int   pq_pop(struct pqueue* q, void* out);              // copies the front element to *out (which may be NULL) and removes it; 0 ok, -1 when empty
const void* pq_peek(const struct pqueue* q);            // the front element, or NULL when empty
unsigned int pq_len(const struct pqueue* q);
```
