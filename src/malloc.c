// Dynamic memory: a first-fit free list over [__heap_start, sp - reserve).
//
// Every block, used or free, starts with an 8-byte header and the blocks are chained in address
// order, each one ending exactly where the next begins. free() merges a block with its free
// neighbours, so two free blocks are never adjacent. See ceres/heap.h for the contract.

#include "ceres/heap.h"
#include "ceres/config.h"
#include "ceres/sys.h"
#include "string.h"
#include "stdio.h"

struct blk
{
    unsigned int size;      // payload bytes (a multiple of 8); bit 0 is the "in use" flag
    struct blk*  next;      // the next block in address order
};

#define BLK_USED    1u
#define BLK_SIZE(b) ((b)->size & ~BLK_USED)
#define ALIGN8(n)   (((n) + 7u) & ~7u)
#define MIN_SPLIT   (sizeof(struct blk) + 8)   // do not leave a remainder that cannot hold a payload
// The largest request malloc considers: anything above it would wrap to a small number when rounded
// up to 8, or when the header is added to it, and be handed a block far smaller than was asked for.
// No machine has that much room anyway (RAM is at most 1 GiB).
#define MAX_REQUEST (0xFFFFFFFFu - 2u * sizeof(struct blk) - 7u)

static struct blk*  heap_head = 0;
static struct blk*  heap_tail = 0;
static char*        heap_brk = 0;               // 0 until the first allocation
static unsigned int heap_reserve = CERES_HEAP_STACK_RESERVE;   // bytes kept free for the stack (ceres/config.h)

void heap_set_stack_reserve(unsigned int bytes)
{
    heap_reserve = bytes;
}

static struct blk* grow(unsigned int payload)
{
    if (heap_brk == 0)
        heap_brk = (char*)ALIGN8(sys_heap_start());

    unsigned int need = payload + sizeof(struct blk);
    unsigned int limit = sys_sp() - heap_reserve;
    unsigned int top = (unsigned int)heap_brk;
    if (top + need > limit || top + need < top)          // out of room, or the sum wrapped
        return 0;

    struct blk* b = (struct blk*)heap_brk;
    heap_brk += need;
    b->size = payload;
    b->next = 0;
    if (heap_tail) heap_tail->next = b;
    else heap_head = b;
    heap_tail = b;
    return b;
}

void* malloc(size_t n)
{
    if (n == 0 || n > MAX_REQUEST)
        return 0;
    n = ALIGN8(n);

    for (struct blk* b = heap_head; b; b = b->next)
    {
        if ((b->size & BLK_USED) == 0 && BLK_SIZE(b) >= n)
        {
            unsigned int spare = BLK_SIZE(b) - n;
            if (spare >= MIN_SPLIT)                       // split off the tail as a new free block
            {
                struct blk* rest = (struct blk*)((char*)(b + 1) + n);
                rest->size = spare - sizeof(struct blk);
                rest->next = b->next;
                b->next = rest;
                if (heap_tail == b) heap_tail = rest;
                b->size = n;
            }
            b->size |= BLK_USED;
            return (void*)(b + 1);
        }
    }

    struct blk* g = grow(n);
    if (!g)
        return 0;
    g->size |= BLK_USED;
    return (void*)(g + 1);
}

void free(void* p)
{
    if (!p)
        return;
    struct blk* self = (struct blk*)p - 1;
    self->size &= ~BLK_USED;

    struct blk* prev = 0;                                 // O(n): a singly linked list has no back pointer
    for (struct blk* b = heap_head; b && b != self; b = b->next)
        prev = b;

    if (self->next && (self->next->size & BLK_USED) == 0)    // absorb the next block
    {
        self->size = BLK_SIZE(self) + sizeof(struct blk) + BLK_SIZE(self->next);
        if (heap_tail == self->next) heap_tail = self;
        self->next = self->next->next;
    }
    if (prev && (prev->size & BLK_USED) == 0)                // ... or be absorbed by the previous one
    {
        prev->size = BLK_SIZE(prev) + sizeof(struct blk) + BLK_SIZE(self);
        if (heap_tail == self) heap_tail = prev;
        prev->next = self->next;
    }
}

void* calloc(size_t n, size_t size)
{
    size_t total = n * size;
    if (size != 0 && total / size != n)                   // n * size overflowed
        return 0;
    void* p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void* realloc(void* p, size_t n)
{
    if (!p)
        return malloc(n);
    if (n == 0)
    {
        free(p);
        return 0;
    }
    struct blk* b = (struct blk*)p - 1;
    if (BLK_SIZE(b) >= n)
        return p;                                         // the block is already big enough
    void* q = malloc(n);
    if (!q)
        return 0;                                         // the original is untouched
    memcpy(q, (const void*)p, BLK_SIZE(b));
    free(p);
    return q;
}

// ---- inspection ----------------------------------------------------------------------

unsigned int heap_used(void)
{
    unsigned int total = 0;
    for (struct blk* b = heap_head; b; b = b->next)
        if (b->size & BLK_USED) total += BLK_SIZE(b);
    return total;
}

unsigned int heap_free(void)
{
    unsigned int total = 0;
    for (struct blk* b = heap_head; b; b = b->next)
        if ((b->size & BLK_USED) == 0) total += BLK_SIZE(b);
    return total;
}

int heap_blocks(void)
{
    int n = 0;
    for (struct blk* b = heap_head; b; b = b->next)
        n++;
    return n;
}

void heap_stats(struct heap_stats* out)
{
    out->start = ALIGN8(sys_heap_start());
    out->brk = heap_brk ? (unsigned int)heap_brk : out->start;
    out->limit = sys_sp() - heap_reserve;
    out->used = 0;
    out->free_bytes = 0;
    out->blocks = 0;
    out->free_blocks = 0;
    out->largest_free = 0;
    for (struct blk* b = heap_head; b; b = b->next)
    {
        out->blocks++;
        if (b->size & BLK_USED)
        {
            out->used += BLK_SIZE(b);
        }
        else
        {
            out->free_bytes += BLK_SIZE(b);
            out->free_blocks++;
            if (BLK_SIZE(b) > out->largest_free) out->largest_free = BLK_SIZE(b);
        }
    }
}

// The invariants free() and malloc() maintain: blocks start 8-aligned, sizes are multiples of 8,
// each block ends exactly where the next begins, the last one ends at brk, the tail pointer is the
// last block, and no two free blocks are adjacent. The first block that breaks one is returned.
int heap_check(void)
{
    if (heap_head == 0)
        return 0;

    unsigned int low = ALIGN8(sys_heap_start());
    unsigned int high = (unsigned int)heap_brk;
    struct blk* prev = 0;

    for (struct blk* b = heap_head; b; b = b->next)
    {
        unsigned int at = (unsigned int)b;
        if (at < low || (at & 7u) != 0 || at + sizeof(struct blk) > high)
            return (int)at;
        if ((b->size & 6u) != 0)                                  // size is not a multiple of 8
            return (int)at;
        unsigned int end = at + sizeof(struct blk) + BLK_SIZE(b);
        if (end > high)
            return (int)at;
        if (b->next ? (unsigned int)b->next != end : end != high)
            return (int)at;
        if (prev && (prev->size & BLK_USED) == 0 && (b->size & BLK_USED) == 0)
            return (int)at;
        prev = b;
    }
    if (prev != heap_tail)
        return (int)(unsigned int)prev;
    return 0;
}

void heap_dump(void)
{
    struct heap_stats s;
    heap_stats(&s);
    putstr("heap "); puthex(s.start); putstr(" .. "); puthex(s.brk);
    putstr(" (limit "); puthex(s.limit); putstr(")\n");
    for (struct blk* b = heap_head; b; b = b->next)
    {
        putstr("  "); puthex((unsigned int)b); putstr(" +");
        putuint(BLK_SIZE(b));
        putstr((b->size & BLK_USED) ? " used\n" : " free\n");
    }
    putuint((unsigned int)s.blocks); putstr(" blocks, ");
    putuint(s.used); putstr(" used, ");
    putuint(s.free_bytes); putstr(" free\n");
}
