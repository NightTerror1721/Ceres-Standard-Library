// Dynamic memory: a two-level segregated fit allocator (TLSF) over [__heap_start, sp - reserve).
//
// Every block, used or free, starts with an 8-byte header, and the blocks tile the heap in address order up to
// an 8-byte epilogue header at the top. A header holds the block's payload size, whether the block is free and
// whether the block just below it is free; a free block also writes its size into the header of the block
// above it (`prev_size`), so free() finds both neighbours in O(1) and merges with them - two free blocks are
// never adjacent. The free blocks are kept in 26 x 8 lists by size: the first level is the size's highest bit,
// the second its next three bits, and two bitmaps say which lists hold anything. malloc takes the first
// non-empty list whose every block is large enough, found with one ctz per level: no search, whatever the heap
// holds. See ceres/heap.h for the contract.
//
// Each time the heap grows its new top becomes the machine's stack limit (sys_set_stack_limit): a stack that
// later runs down into the heap is a StackOverflow, reported like any other, instead of allocations quietly
// rewritten by stack frames.
//
// With CERES_HEAP_DEBUG (ceres/config.h) every block carries a canary after what was asked for, a freed block
// is filled with 0xDD, and heap_check() looks at both: an overrun, a write after free, a double free or a free
// of a pointer malloc did not return is reported through the heap error handler.

#include "ceres/heap.h"
#include "ceres/config.h"
#include "ceres/sys.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "errno.h"
#include "ceres/terminal.h"

struct blk
{
    unsigned int prev_size;   // the payload size of the block just below, while that one is free (B_PREV_FREE)
    unsigned int head;        // this block's payload size, a multiple of 8, | B_FREE | B_PREV_FREE
    struct blk*  next_free;   // only while free: the neighbours in its size's list
    struct blk*  prev_free;
};

#define HDR           8u                      // prev_size and head
#define B_FREE        1u
#define B_PREV_FREE   2u
#define SIZE(b)       ((b)->head & ~7u)
#define PAYLOAD(b)    ((char*)(b) + HDR)
#define OF(p)         ((struct blk*)((char*)(p) - HDR))
#define NEXT_PHYS(b)  ((struct blk*)(PAYLOAD(b) + SIZE(b)))
#define PREV_PHYS(b)  ((struct blk*)((char*)(b) - HDR - (b)->prev_size))
#define ALIGN8(n)     (((n) + 7u) & ~7u)
#define MIN_PAYLOAD   8u                      // room for the two list pointers of a free block
#define MIN_SPLIT     (HDR + MIN_PAYLOAD)     // a remainder smaller than this stays with the block
// The largest request: 1 GiB, all a machine can have. It keeps every sum below free of wrapping.
#define MAX_REQUEST   0x40000000u

#if CERES_HEAP_DEBUG
#define TRAILER       8u                      // the canary bytes and, in the last word, the size asked for
#define FREE_FILL     0xDD
#define CANARY        0xCA
#else
#define TRAILER       0u
#endif

// ---- the size classes ------------------------------------------------------------------------

#define SL_LOG2   3                           // 8 lists per power of two
#define SL_COUNT  8
#define FL_COUNT  26                          // payloads up to 2^31
#define SMALL     64u                         // below this the lists are exact: 8, 16, ... 56

static struct blk*  bins[208];                // FL_COUNT * SL_COUNT heads
static unsigned int fl_map;                   // bit f: some list of first level f holds a block
static unsigned int sl_map[26];               // bit s of word f: list (f, s) holds a block

static int top_bit(unsigned int x)
{
    return 31 - __builtin_clz(x);
}

// The list a block of `size` payload bytes belongs in, as first level * 8 + second level.
static int bin_of(unsigned int size)
{
    if (size < SMALL)
        return (int)(size >> 3);
    int top = top_bit(size);
    return ((top - (SL_LOG2 + 2)) << SL_LOG2) | (int)((size >> (top - SL_LOG2)) & (SL_COUNT - 1));   // 64 is first level 1
}

static void unlink_free(struct blk* b)
{
    int i = bin_of(SIZE(b));
    struct blk* next = b->next_free;
    struct blk* prev = b->prev_free;
    if (prev) prev->next_free = next;
    else bins[i] = next;
    if (next) next->prev_free = prev;
    else if (prev == 0)                       // the list is now empty
    {
        int fl = i >> SL_LOG2;
        sl_map[fl] &= ~(1u << (i & (SL_COUNT - 1)));
        if (sl_map[fl] == 0)
            fl_map &= ~(1u << fl);
    }
}

// A free block whose every member is at least `size` bytes, or 0. The size is rounded up to the next list
// boundary first, so that the list found holds nothing too small: good fit, not best fit, and O(1).
static struct blk* find(unsigned int size)
{
    if (size >= SMALL)
        size += (1u << (top_bit(size) - SL_LOG2)) - 1u;
    int i = bin_of(size);
    int fl = i >> SL_LOG2;
    int sl = i & (SL_COUNT - 1);
    if (fl >= FL_COUNT)
        return 0;
    unsigned int sl_bits = sl_map[fl] & (~0u << sl);
    if (sl_bits == 0)
    {
        unsigned int fl_bits = fl + 1 < 32 ? fl_map & (~0u << (fl + 1)) : 0u;
        if (fl_bits == 0)
            return 0;
        fl = __builtin_ctz(fl_bits);
        sl_bits = sl_map[fl];
    }
    return bins[fl * SL_COUNT + __builtin_ctz(sl_bits)];
}

// ---- the heap's edges --------------------------------------------------------------------------

static struct blk*  heap_first = 0;           // the lowest block, 0 until the first allocation
static struct blk*  heap_epilogue = 0;        // the size-0 header above the last block
static unsigned int heap_reserve = CERES_HEAP_STACK_RESERVE;   // bytes kept free for the stack (ceres/config.h)

void heap_set_stack_reserve(unsigned int bytes)
{
    heap_reserve = bytes;
}

// The highest address the heap may reach: `heap_reserve` bytes below the stack pointer. A reserve
// larger than sp (a machine started with little --memory, or a big heap_set_stack_reserve) leaves no
// room at all, rather than wrapping round to an address near the top of the address space.
static unsigned int heap_limit(void)
{
    unsigned int sp = sys_sp();
    return sp > heap_reserve ? sp - heap_reserve : 0u;
}

static unsigned int heap_brk(void)
{
    return (unsigned int)heap_epilogue + HDR;
}

static int start_heap(void)
{
    if (heap_first != 0)
        return 1;
    unsigned int start = ALIGN8(sys_heap_start());
    if (start + HDR > heap_limit())
        return 0;
    heap_first = (struct blk*)start;
    heap_epilogue = heap_first;
    heap_epilogue->prev_size = 0;
    heap_epilogue->head = 0;
    sys_set_stack_limit(heap_brk());
    return 1;
}

// Moves the epilogue to `to` (8-aligned, above the last block), if the heap may reach that far.
static int move_top(char* to)
{
    unsigned int top = (unsigned int)to;
    if (top < (unsigned int)heap_first || top + HDR > heap_limit())
        return 0;
    heap_epilogue = (struct blk*)to;
    heap_epilogue->prev_size = 0;
    heap_epilogue->head = 0;
    sys_set_stack_limit(heap_brk());          // the stack stops where the heap now ends
    return 1;
}

// A used block of at least `size` bytes made at the top of the heap: the free block that ends there is
// extended, or a new block goes where the epilogue was.
static struct blk* grow(unsigned int size)
{
    struct blk* b = heap_epilogue;
    unsigned int flags = 0;
    if (b->head & B_PREV_FREE)
    {
        b = PREV_PHYS(b);                     // the last block is free: take it and extend it
        flags = b->head & B_PREV_FREE;
        if (SIZE(b) >= size)
        {
            unlink_free(b);
            b->head &= ~B_FREE;
            heap_epilogue->head &= ~B_PREV_FREE;
            return b;
        }
    }
    else
    {
        flags = b->head & B_PREV_FREE;
    }
    if ((unsigned int)PAYLOAD(b) + size < (unsigned int)PAYLOAD(b))   // wrapped: no machine has that
        return 0;
    struct blk* old_epilogue = heap_epilogue;
    if (!move_top(PAYLOAD(b) + size))
        return 0;
    if (b != old_epilogue)
        unlink_free(b);
    b->head = size | flags;
    return b;
}

// ---- free blocks ---------------------------------------------------------------------------

#if CERES_HEAP_DEBUG
static void fill_free(struct blk* b)
{
    if (SIZE(b) > 8u)
        memset(PAYLOAD(b) + 8, FREE_FILL, SIZE(b) - 8u);
}
#endif

// Puts a block in its list and tells the block above that it is free. The block is not adjacent to another free
// block: the callers merge first.
static void insert(struct blk* b)
{
    b->head |= B_FREE;
    struct blk* next = NEXT_PHYS(b);
    next->prev_size = SIZE(b);
    next->head |= B_PREV_FREE;
#if CERES_HEAP_DEBUG
    fill_free(b);
#endif
    int i = bin_of(SIZE(b));
    int fl = i >> SL_LOG2;
    b->prev_free = 0;
    b->next_free = bins[i];
    if (bins[i]) bins[i]->prev_free = b;
    bins[i] = b;
    sl_map[fl] |= 1u << (i & (SL_COUNT - 1));
    fl_map |= 1u << fl;
}

// Frees a used block: merges it with a free neighbour on either side, and lists what results.
static void release(struct blk* b)
{
    struct blk* next = NEXT_PHYS(b);
    if (next->head & B_FREE)
    {
        unlink_free(next);
        b->head += HDR + SIZE(next);
    }
    if (b->head & B_PREV_FREE)
    {
        struct blk* prev = PREV_PHYS(b);
        unlink_free(prev);
        prev->head = (prev->head & ~B_FREE) + HDR + SIZE(b);
        b = prev;
    }
    insert(b);
}

// Cuts a used block down to `size` bytes when what is left over makes a block of its own, and frees that.
static void trim(struct blk* b, unsigned int size)
{
    if (SIZE(b) - size < MIN_SPLIT)
        return;
    struct blk* rest = (struct blk*)(PAYLOAD(b) + size);
    rest->head = SIZE(b) - size - HDR;        // used, and the block below it (b) is used
    b->head = size | (b->head & B_PREV_FREE);
    release(rest);
}

// ---- errors --------------------------------------------------------------------------------

void __abort_now(void) __attribute__((__noreturn__));

static void err(const char* s)
{
    term_write_error(s, (int)strlen(s));
}

static void report_and_abort(const char* what, void* p)
{
    char hex[11] = "0x00000000";
    for (int i = 0; i < 8; i++)
        hex[9 - i] = "0123456789abcdef"[((unsigned int)p >> (4 * i)) & 15u];
    err("heap: ");                            // on the error stream, like assert
    err(what);
    err(" at ");
    err(hex);
    err("\n");
    __abort_now();                            // SIGABRT's handler, if any, then status 134
}

static heap_error_fn heap_error = report_and_abort;

heap_error_fn heap_set_error_handler(heap_error_fn f)
{
    heap_error_fn old = heap_error;
    heap_error = f != 0 ? f : report_and_abort;
    return old;
}

// Whether p can be a payload of this heap: 8-aligned and between its first block and its top.
static int in_heap(void* p)
{
    unsigned int at = (unsigned int)p;
    return heap_first != 0 && (at & 7u) == 0 && at >= (unsigned int)PAYLOAD(heap_first) &&
        at <= (unsigned int)heap_epilogue;
}

// Whether p is the payload of a block malloc handed out and has not taken back: inside the heap, and its header
// agreeing with the headers around it. A pointer into the middle of a block, or one freed and since swallowed by
// a free neighbour below it, fails one of these instead of being trusted as a header.
static int live_block(void* p)
{
    if (!in_heap(p))
        return 0;
    struct blk* b = OF(p);
    if ((b->head & (B_FREE | 4u)) != 0 || SIZE(b) < MIN_PAYLOAD)
        return 0;
    unsigned int end = (unsigned int)PAYLOAD(b) + SIZE(b);
    if (end < (unsigned int)PAYLOAD(b) || end > (unsigned int)heap_epilogue)
        return 0;
    if (NEXT_PHYS(b)->head & B_PREV_FREE)
        return 0;                                          // the block above takes this one for free
    if (b->head & B_PREV_FREE)
    {
        unsigned int below = (unsigned int)b - HDR - b->prev_size;
        if (b->prev_size < MIN_PAYLOAD || below < (unsigned int)heap_first || below >= (unsigned int)b)
            return 0;
        struct blk* prev = PREV_PHYS(b);
        if ((prev->head & B_FREE) == 0 || SIZE(prev) != b->prev_size)
            return 0;
    }
    return 1;
}

#if CERES_HEAP_DEBUG
// The debug trailer: canary bytes from what was asked for up to the last word, which holds that size.
static void arm(struct blk* b, size_t n)
{
    unsigned int end = SIZE(b) - 4u;
    memset(PAYLOAD(b) + n, CANARY, end - n);
    *(unsigned int*)(PAYLOAD(b) + end) = (unsigned int)n;
}

static int canary_intact(struct blk* b)
{
    unsigned int end = SIZE(b) - 4u;
    unsigned int n = *(const unsigned int*)(PAYLOAD(b) + end);
    if (n > end - 4u)                                     // the size word itself was overwritten
        return 0;
    const unsigned char* c = (const unsigned char*)PAYLOAD(b);
    for (unsigned int i = n; i < end; i++)
        if (c[i] != CANARY)
            return 0;
    return 1;
}

static int fill_intact(struct blk* b)
{
    const unsigned char* c = (const unsigned char*)PAYLOAD(b);
    for (unsigned int i = 8; i < SIZE(b); i++)
        if (c[i] != FREE_FILL)
            return 0;
    return 1;
}
#define ARM(b, n) arm((b), (n))
#else
#define ARM(b, n) ((void)0)
#endif

// ---- the interface ---------------------------------------------------------------------------

// The payload bytes a request of n takes: rounded to 8, room for the list pointers once it is freed, and
// the debug trailer.
static unsigned int block_size(size_t n)
{
    unsigned int size = ALIGN8((unsigned int)n) + TRAILER;
    return size < MIN_PAYLOAD ? MIN_PAYLOAD : size;
}

// A used block of at least `size` bytes, from a list or from the top of the heap; 0 when neither has room.
static struct blk* take(unsigned int size)
{
    if (!start_heap())
        return 0;
    struct blk* b = find(size);
    if (b != 0)
    {
        unlink_free(b);
        b->head &= ~B_FREE;
        NEXT_PHYS(b)->head &= ~B_PREV_FREE;
        return b;
    }
    return grow(size);
}

void* malloc(size_t n)
{
    if (n == 0)
        return 0;
    if (n > MAX_REQUEST)
    {
        errno = ENOMEM;
        return 0;
    }
    unsigned int size = block_size(n);
    struct blk* b = take(size);
    if (b == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    trim(b, size);
    ARM(b, n);
    return PAYLOAD(b);
}

void free(void* p)
{
    if (!p)
        return;
    struct blk* b = OF(p);
    if (!in_heap(p))
    {
        heap_error("free of a pointer malloc did not return", p);
        return;
    }
    if (b->head & B_FREE)
    {
        heap_error("double free", p);
        return;
    }
    if (!live_block(p))
    {
        heap_error("free of a pointer that is not a live block", p);   // inside a block, or freed and merged away
        return;
    }
#if CERES_HEAP_DEBUG
    if (!canary_intact(b))
    {
        heap_error("write past the end of a block", p);
        return;
    }
#endif
    release(b);
}

void* calloc(size_t n, size_t size)
{
    if (size != 0 && n > (size_t)-1 / size)               // n * size overflowed
    {
        errno = ENOMEM;
        return 0;
    }
    size_t total = n * size;
    void* p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

size_t malloc_usable_size(void* p)
{
    if (p == 0 || !live_block(p))
        return 0;
    return SIZE(OF(p)) - TRAILER;
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
    struct blk* b = OF(p);
    if (!live_block(p))
    {
        heap_error(!in_heap(p) ? "realloc of a pointer malloc did not return" :
            (b->head & B_FREE) ? "realloc of a freed block" : "realloc of a pointer that is not a live block", p);
        return 0;
    }
    if (n > MAX_REQUEST)
    {
        errno = ENOMEM;
        return 0;                                         // the original is untouched
    }
#if CERES_HEAP_DEBUG
    if (!canary_intact(b))
    {
        heap_error("write past the end of a block", p);
        return 0;
    }
#endif
    unsigned int size = block_size(n);
    if (size <= SIZE(b))                                  // shrinking, or within the block: it stays
    {
        trim(b, size);
        ARM(b, n);
        return p;
    }
    // Growing in place: into a free block above, or past the top of the heap.
    struct blk* next = NEXT_PHYS(b);
    unsigned int room = SIZE(b);
    struct blk* above = next;
    if (next->head & B_FREE)
    {
        room += HDR + SIZE(next);
        above = NEXT_PHYS(next);
    }
    if ((next->head & B_FREE) && room >= size)
    {
        unlink_free(next);
        b->head += HDR + SIZE(next);
        NEXT_PHYS(b)->head &= ~B_PREV_FREE;
        trim(b, size);
        ARM(b, n);
        return p;
    }
    if (above == heap_epilogue && (unsigned int)PAYLOAD(b) + size > (unsigned int)PAYLOAD(b))
    {
        struct blk* old_next = next;
        if (move_top(PAYLOAD(b) + size))
        {
            if (old_next != above)
                unlink_free(old_next);            // the free block between b and the top becomes b's
            b->head = size | (b->head & B_PREV_FREE);
            ARM(b, n);
            return p;
        }
    }
    void* q = malloc(n);
    if (!q)
        return 0;                                         // the original is untouched
    memcpy(q, (const void*)p, SIZE(b) - TRAILER);
    free(p);
    return q;
}

void* aligned_alloc(size_t alignment, size_t n)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        errno = EINVAL;
        return 0;
    }
    if (alignment > MAX_REQUEST)
    {
        errno = ENOMEM;                                    // a valid alignment no machine has room for
        return 0;
    }
    if (alignment <= 8)
        return malloc(n);
    if (n == 0)
        return 0;
    if (n > MAX_REQUEST)
    {
        errno = ENOMEM;
        return 0;
    }
    unsigned int size = block_size(n);
    // Enough for the size, plus a front piece of up to alignment + MIN_SPLIT - 8 bytes to cut off and free.
    struct blk* b = take(size + (unsigned int)alignment + MIN_SPLIT);
    if (b == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    unsigned int at = (unsigned int)PAYLOAD(b);
    unsigned int want = (at + (unsigned int)alignment - 1u) & ~((unsigned int)alignment - 1u);
    if (want != at)
    {
        if (want - at < MIN_SPLIT)
            want += (unsigned int)alignment;              // the front piece has to be a block of its own
        unsigned int front = want - at;
        struct blk* nb = OF((void*)want);
        nb->head = SIZE(b) - front;                       // used, and its lower neighbour is used until freed
        b->head = (front - HDR) | (b->head & B_PREV_FREE);
        release(b);
        b = nb;
    }
    trim(b, size);
    ARM(b, n);
    return PAYLOAD(b);
}

int posix_memalign(void** out, size_t alignment, size_t n)
{
    if (out == 0 || alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0)
        return EINVAL;
    int saved = errno;
    void* p = n == 0 ? 0 : aligned_alloc(alignment, n);
    int err = errno;
    errno = saved;
    if (n != 0 && p == 0)
        return err == EINVAL ? EINVAL : ENOMEM;
    *out = p;
    return 0;
}

// ---- inspection ----------------------------------------------------------------------

void heap_stats(struct heap_stats* out)
{
    out->start = heap_first ? (unsigned int)heap_first : ALIGN8(sys_heap_start());
    out->brk = heap_first ? heap_brk() : out->start;
    out->limit = heap_limit();
    out->used = 0;
    out->free_bytes = 0;
    out->blocks = 0;
    out->free_blocks = 0;
    out->largest_free = 0;
    if (heap_first == 0)
        return;
    for (struct blk* b = heap_first; b != heap_epilogue; b = NEXT_PHYS(b))
    {
        out->blocks++;
        if (b->head & B_FREE)
        {
            out->free_bytes += SIZE(b) - TRAILER;
            out->free_blocks++;
            if (SIZE(b) - TRAILER > out->largest_free) out->largest_free = SIZE(b) - TRAILER;
        }
        else
        {
            out->used += SIZE(b) - TRAILER;
        }
    }
}

unsigned int heap_used(void)
{
    struct heap_stats s;
    heap_stats(&s);
    return s.used;
}

unsigned int heap_free(void)
{
    struct heap_stats s;
    heap_stats(&s);
    return s.free_bytes;
}

int heap_blocks(void)
{
    struct heap_stats s;
    heap_stats(&s);
    return s.blocks;
}

// The invariants the allocator maintains: blocks are 8-aligned with sizes a multiple of 8 and at least 8, each
// ends where the next begins, the last one ends at the epilogue, no two free blocks are adjacent, a free
// block's size is written in the header above it and that header says so, and the lists hold exactly the free
// blocks, each in the list of its size. With CERES_HEAP_DEBUG, used blocks keep their canary and free ones their
// 0xDD fill. The first block that breaks one is returned.
int heap_check(void)
{
    if (heap_first == 0)
        return 0;

    unsigned int low = (unsigned int)heap_first;
    unsigned int high = (unsigned int)heap_epilogue;
    int prev_free = 0;
    int free_blocks = 0;
    struct blk* b = heap_first;
    while (b != heap_epilogue)
    {
        unsigned int at = (unsigned int)b;
        if (at < low || at > high || (at & 7u) != 0 || SIZE(b) < MIN_PAYLOAD || (b->head & 4u) != 0)
            return (int)at;
        if (SIZE(b) > high - at - HDR)                    // runs past the top
            return (int)at;
        if (((b->head & B_PREV_FREE) != 0) != prev_free)
            return (int)at;
        int is_free = (b->head & B_FREE) != 0;
        if (is_free && prev_free)
            return (int)at;
        struct blk* next = NEXT_PHYS(b);
        if (is_free)
        {
            free_blocks++;
            if (next->prev_size != SIZE(b))
                return (int)at;
#if CERES_HEAP_DEBUG
            if (!fill_intact(b))
                return (int)at;
#endif
        }
        else
        {
#if CERES_HEAP_DEBUG
            if (!canary_intact(b))
                return (int)at;
#endif
        }
        prev_free = is_free;
        b = next;
    }
    if (((heap_epilogue->head & B_PREV_FREE) != 0) != prev_free || SIZE(heap_epilogue) != 0)
        return (int)(unsigned int)heap_epilogue;

    int listed = 0;
    for (int fl = 0; fl < FL_COUNT; fl++)
    {
        for (int sl = 0; sl < SL_COUNT; sl++)
        {
            struct blk* head = bins[fl * SL_COUNT + sl];
            int has = (sl_map[fl] & (1u << sl)) != 0;
            if ((head != 0) != has || (has && (fl_map & (1u << fl)) == 0))
                return head ? (int)(unsigned int)head : -1;
            struct blk* before = 0;
            for (struct blk* f = head; f; f = f->next_free)
            {
                unsigned int at = (unsigned int)f;
                if (at < low || at >= high || (f->head & B_FREE) == 0 || f->prev_free != before ||
                    bin_of(SIZE(f)) != fl * SL_COUNT + sl || ++listed > free_blocks)
                    return (int)at;
                before = f;
            }
        }
        if (sl_map[fl] == 0 && (fl_map & (1u << fl)) != 0)
            return -1;
    }
    if (listed != free_blocks)
        return -1;
    return 0;
}

void heap_dump(void)
{
    struct heap_stats s;
    heap_stats(&s);
    putstr("heap "); puthex(s.start); putstr(" .. "); puthex(s.brk);
    putstr(" (limit "); puthex(s.limit); putstr(")\n");
    if (heap_first != 0)
    {
        for (struct blk* b = heap_first; b != heap_epilogue; b = NEXT_PHYS(b))
        {
            putstr("  "); puthex((unsigned int)b); putstr(" +");
            putuint(SIZE(b) - TRAILER);
            putstr((b->head & B_FREE) ? " free\n" : " used\n");
        }
    }
    putuint((unsigned int)s.blocks); putstr(" blocks, ");
    putuint(s.used); putstr(" used, ");
    putuint(s.free_bytes); putstr(" free\n");
}
