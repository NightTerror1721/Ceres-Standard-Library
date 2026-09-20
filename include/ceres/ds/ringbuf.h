#pragma once

// A first-in, first-out queue of bytes in a fixed buffer: the "input event buffer" - an interrupt handler
// pushes, the main loop drains.
//
// It is safe for ONE producer and ONE consumer without masking interrupts, because each index has one
// writer: the producer writes only `head`, the consumer only `tail`. Both are volatile and run freely
// (they are masked on use, not on store), so the buffer holds all `size` bytes and full and empty are
// told apart without a spare slot. The size must be a power of two so that masking replaces a division.

struct ringbuf
{
    unsigned char* buf;
    unsigned int mask;                  // size - 1
    volatile unsigned int head;         // bytes ever put (written by the producer)
    volatile unsigned int tail;         // bytes ever taken (written by the consumer)
};

// `storage` is `size` bytes and `size` a power of two (a size that is not one is rounded DOWN to one).
void ring_init(struct ringbuf* r, void* storage, unsigned int size);
int  ring_put(struct ringbuf* r, unsigned char b);          // 0 ok, -1 when full (the byte is not stored)
int  ring_get(struct ringbuf* r);                           // the next byte, or -1 when empty
int  ring_peek(const struct ringbuf* r);                    // the next byte without taking it, or -1
unsigned int ring_count(const struct ringbuf* r);           // bytes waiting
unsigned int ring_space(const struct ringbuf* r);           // bytes that still fit
unsigned int ring_write(struct ringbuf* r, const void* data, unsigned int n);   // as many as fit; returns how many
unsigned int ring_read(struct ringbuf* r, void* out, unsigned int n);           // as many as there are; returns how many
void ring_clear(struct ringbuf* r);                         // consumer side: drops what is waiting
