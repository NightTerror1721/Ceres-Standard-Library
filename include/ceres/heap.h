#pragma once

#include "stddef.h"

// Dynamic memory. The heap is the ground between the end of the image (__heap_start) and the
// stack; it grows upward on demand and stops `heap_set_stack_reserve()` bytes short of sp.
// malloc and free take the same time however many blocks the heap holds: the free blocks are kept in lists
// by size (a two-level segregated fit), a block is split when it is larger than needed, and free() merges it
// with its free neighbours at once, through the size every free block leaves in the header above it.
// realloc grows a block in place when the block above it is free or it is the last one, and shrinks it in
// place. Payloads are 8-byte aligned, the header is 8 bytes, and the smallest payload is 8.
//
// The stack cannot run into the heap: each time the heap grows, malloc makes its new top the machine's
// stack limit (sys_set_stack_limit, CeresASM fcf7d4c), and a stack that comes down to it is a
// StackOverflow instead of rewritten allocations. The reserve is what malloc leaves the stack to grow into.

void*  malloc(size_t n);               // NULL when n == 0 or nothing fits
void*  calloc(size_t n, size_t size);  // zeroed; NULL on overflow of n * size
void*  realloc(void* p, size_t n);     // realloc(NULL, n) == malloc(n); realloc(p, 0) frees
void   free(void* p);                  // free(NULL) is a no-op
void*  aligned_alloc(size_t alignment, size_t n);   // alignment a power of two; NULL and EINVAL otherwise
int    posix_memalign(void** out, size_t alignment, size_t n);   // 0, EINVAL or ENOMEM
size_t malloc_usable_size(void* p);    // what the block can hold: at least what was asked for

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

// What malloc does when it finds a misuse: a double free, a free or realloc of a pointer it did not return,
// and with CERES_HEAP_DEBUG an overrun past a block's end. `what` says which and `p` is the pointer. The
// default prints "heap: <what> at <p>" on the error stream and aborts (SIGABRT, status 134); a handler that returns makes the
// call that found it do nothing. Returns the previous handler; NULL restores the default.
typedef void (*heap_error_fn)(const char* what, void* p);
heap_error_fn heap_set_error_handler(heap_error_fn f);
