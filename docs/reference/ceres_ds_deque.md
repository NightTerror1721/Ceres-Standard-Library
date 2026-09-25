# `<ceres/ds/deque.h>`

A double-ended queue of fixed-size elements that grows: push and pop at EITHER end in O(1) amortized, and dq_at is random access by logical index. Neither vector.h (only grows from the end) nor ringbuf.h (bytes only, fixed capacity, one producer and one consumer) covers this - a work queue that is pushed and popped from both ends, or a bounded undo/redo history, wants this one.

```c
struct deque q;  dq_init(&q, sizeof(int));
int v = 3;  dq_push_back(&q, &v);  dq_push_front(&q, &v);
int out;  dq_pop_front(&q, &out);
dq_free(&q);
```

Growing may move the storage: a pointer from dq_at is good until the next push or pop.

```c
struct deque
{
    void* data;
    unsigned int head;         // index of the front element within data, wrapping mod cap
    unsigned int len;
    unsigned int cap;
    unsigned int elem;         // bytes per element
};

void  dq_init(struct deque* d, unsigned int elem_size);
void  dq_free(struct deque* d);                                   // releases the storage; the deque is empty and usable again
void  dq_clear(struct deque* d);                                  // len = 0, keeps the storage
int   dq_push_back(struct deque* d, const void* item);            // 0 ok, -1 when out of memory
int   dq_push_front(struct deque* d, const void* item);           // 0 ok, -1 when out of memory
int   dq_pop_back(struct deque* d, void* out);                    // copies to *out (may be NULL) and removes it; 0 ok, -1 when empty
int   dq_pop_front(struct deque* d, void* out);                   // 0 ok, -1 when empty
void* dq_at(const struct deque* d, unsigned int i);                // 0 == the front; NULL when i >= len

static inline unsigned int dq_len(const struct deque* d) { return d->len; }
static inline int dq_empty(const struct deque* d) { return d->len == 0; }

// Typed access, unchecked like VECTOR_GET: i must be < len.
#define DEQUE_GET(d, T, i) (*((T*)dq_at((d), (i))))
```
