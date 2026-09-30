// Byte FIFO with one writer per index, over ceres/ds/cqueue.h (a queue of one-byte elements). See ringbuf.h.
#include "ceres/ds/ringbuf.h"

void ring_init(struct ringbuf* r, void* storage, unsigned int size)
{
    cq_init(&r->q, storage, 1u, size);
}

unsigned int ring_count(const struct ringbuf* r) { return cq_count(&r->q); }
unsigned int ring_space(const struct ringbuf* r) { return cq_space(&r->q); }

int ring_put(struct ringbuf* r, unsigned char b)
{
    return cq_put(&r->q, &b);
}

int ring_get(struct ringbuf* r)
{
    unsigned char b;
    return cq_get(&r->q, &b) == 0 ? (int)b : -1;
}

int ring_peek(const struct ringbuf* r)
{
    unsigned char b;
    return cq_peek(&r->q, &b) == 0 ? (int)b : -1;
}

unsigned int ring_write(struct ringbuf* r, const void* data, unsigned int n)
{
    const unsigned char* p = (const unsigned char*)data;
    unsigned int done = 0;
    while (done < n && ring_put(r, p[done]) == 0)
        done++;
    return done;
}

unsigned int ring_read(struct ringbuf* r, void* out, unsigned int n)
{
    unsigned char* p = (unsigned char*)out;
    unsigned int done = 0;
    while (done < n)
    {
        int b = ring_get(r);
        if (b < 0)
            break;
        p[done++] = (unsigned char)b;
    }
    return done;
}

void ring_clear(struct ringbuf* r)
{
    cq_clear(&r->q);
}
