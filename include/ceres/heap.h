#pragma once

#include "stddef.h"

// Dynamic memory. The heap is the ground between the end of the image (__heap_start) and the
// stack; it grows upward on demand and stops `heap_set_stack_reserve()` bytes short of sp.
// It is a first-fit free list in address order: blocks are split when they are larger than needed
// and merged with free neighbours on free(). Payloads are 8-byte aligned, the header is 8 bytes.
//
// The machine does not protect the heap from the stack: a stack that grows past the reserve
// silently overwrites it.

void*  malloc(size_t n);               // NULL when n == 0 or nothing fits
void*  calloc(size_t n, size_t size);  // zeroed; NULL on overflow of n * size
void*  realloc(void* p, size_t n);     // realloc(NULL, n) == malloc(n); realloc(p, 0) frees
void   free(void* p);                  // free(NULL) is a no-op

struct heap_stats
{
    unsigned int start;          // the first address of the heap
    unsigned int brk;            // how far it has grown
    unsigned int limit;          // the highest address it may reach: sp - reserve
    unsigned int used;           // payload bytes in use
    unsigned int free_bytes;     // payload bytes in free blocks
    int blocks;                  // blocks in total
    int free_blocks;
    unsigned int largest_free;   // the biggest single request that fits without growing
};

void         heap_set_stack_reserve(unsigned int bytes);   // default 16 KiB; a reserve above sp leaves the heap no room
unsigned int heap_used(void);
unsigned int heap_free(void);
int          heap_blocks(void);
void         heap_stats(struct heap_stats* out);
int          heap_check(void);   // 0 when the block list is sound, else the address of the first bad block
void         heap_dump(void);    // one line per block on the terminal
