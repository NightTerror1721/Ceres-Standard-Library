#pragma once

// A fixed-capacity circular queue of fixed-size elements, over a buffer the caller provides: the
// ringbuf.h idea generalized from bytes to a struct-sized item, for an interrupt handler that
// cannot call malloc but has more to hand off than one byte - a keyboard event, a packet from a
// peripheral. Like ringbuf.h, safe for ONE producer and ONE consumer without masking interrupts:
// each index has one writer, both run freely and are masked on use, not on store. The capacity
// must be a power of two so masking replaces a division.

struct cqueue
{
    unsigned char* buf;
    unsigned int elem;                  // bytes per element
    unsigned int mask;                  // capacity - 1, in elements
    volatile unsigned int head;         // elements ever put (written by the producer)
    volatile unsigned int tail;         // elements ever taken (written by the consumer)
};

// `storage` is at least elem_size * count bytes; count (a number of elements, not bytes) is rounded
// DOWN to a power of two, at least 1.
void cq_init(struct cqueue* q, void* storage, unsigned int elem_size, unsigned int count);
int  cq_put(struct cqueue* q, const void* item);            // 0 ok, -1 when full (the item is not stored)
int  cq_get(struct cqueue* q, void* out);                   // 0 ok, -1 when empty (*out untouched)
unsigned int cq_count(const struct cqueue* q);               // elements waiting
unsigned int cq_space(const struct cqueue* q);                // elements that still fit
void cq_clear(struct cqueue* q);                             // consumer side: drops what is waiting
