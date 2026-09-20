// Byte FIFO with one writer per index. See ceres/ds/ringbuf.h.
#include "ceres/ds/ringbuf.h"
#include "ceres/bits.h"

void ring_init(struct ringbuf* r, void* storage, unsigned int size)
{
    unsigned int pow2 = 1;
    if (size > 1)
        pow2 = 1u << bit_log2(size);                    // the largest power of two that fits
    r->buf = (unsigned char*)storage;
    r->mask = pow2 - 1u;
    r->head = 0;
    r->tail = 0;
}

unsigned int ring_count(const struct ringbuf* r)
{
    return r->head - r->tail;                           // wraps correctly: both run freely
}

unsigned int ring_space(const struct ringbuf* r)
{
    return r->mask + 1u - ring_count(r);
}

int ring_put(struct ringbuf* r, unsigned char b)
{
    unsigned int head = r->head;
    if (head - r->tail > r->mask)                       // size bytes waiting: full
        return -1;
    r->buf[head & r->mask] = b;
    r->head = head + 1u;                                // published only after the byte is in place
    return 0;
}

int ring_get(struct ringbuf* r)
{
    unsigned int tail = r->tail;
    if (r->head == tail)
        return -1;
    unsigned char b = r->buf[tail & r->mask];
    r->tail = tail + 1u;
    return b;
}

int ring_peek(const struct ringbuf* r)
{
    unsigned int tail = r->tail;
    if (r->head == tail)
        return -1;
    return r->buf[tail & r->mask];
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
    r->tail = r->head;
}
