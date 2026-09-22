#pragma once

#include "vector.h"

// A binary heap like ceres/ds/pqueue.h, plus what pqueue.h cannot do: lower an element's priority
// after it is already inside, or take a specific element out before its turn - both O(log n),
// which is what Dijkstra and A* need for "this neighbor is now reachable more cheaply" and
// "abandon this node". pqueue.h cannot do either because once an element is copied in, nothing
// says where it ended up.
//
// A handle from ih_push stays valid until the element it names is popped or ih_remove'd - it
// never changes even as the heap itself reorders around it, because a second array (`pos`) tracks
// where each handle currently sits.
//
//   struct iheap h;  ih_init(&h, sizeof(float), cmp_float);
//   float d = 5.0f;  unsigned int handle = ih_push(&h, &d);
//   float better = 2.0f;  ih_decrease(&h, handle, &better);
//   float out;  ih_pop(&h, &out);
//
// cmp compares two ITEMS, the same convention as pqueue.h: cmp(a, b) < 0 means a leaves first.

struct iheap
{
    struct vector items;       // elem_size bytes each, indexed by HANDLE (not heap position)
    struct vector heap;        // one unsigned int per heap slot: the handle currently sitting there
    struct vector pos;         // one unsigned int per HANDLE: its current heap slot, or IH_NOT_IN_HEAP
    int (*cmp)(const void*, const void*);
};

#define IH_NOT_IN_HEAP 0xFFFFFFFFu

void ih_init(struct iheap* h, unsigned int elem_size, int (*cmp)(const void*, const void*));
void ih_free(struct iheap* h);
unsigned int ih_len(const struct iheap* h);                          // elements currently in the heap
const void* ih_peek(const struct iheap* h);                          // the front element, or NULL when empty

// Copies *item in and returns its handle, or IH_NOT_IN_HEAP when out of memory. Handles are never
// reused: every push gets a new one, even across pops.
unsigned int ih_push(struct iheap* h, const void* item);
int  ih_pop(struct iheap* h, void* out);                             // copies the front element to *out (may be NULL) and removes it; 0 ok, -1 when empty

// Replaces handle's item and re-heapifies - works whether the new item sorts earlier or later than
// the old one, not only on a genuine decrease, at no extra cost. Does nothing if handle has already
// been popped or removed.
void ih_decrease(struct iheap* h, unsigned int handle, const void* new_item);
void ih_remove(struct iheap* h, unsigned int handle);                // takes handle out before its turn; does nothing if it already left
