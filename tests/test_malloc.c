// The allocator: a randomized stress run with a fill pattern, then the edges.
// Addresses are never printed - they change with the optimization level.
#include "ceres/test.h"
#include "ceres/heap.h"
#include "ceres/config.h"
#include "ceres/timer.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

// Instructions for 64 malloc/free pairs of assorted sizes.
static int pair_cost(void)
{
    uint64_t start = timer_cycles64();
    for (int i = 0; i < 64; i++)
        free(malloc((size_t)(16 + i * 8)));
    return (int)(timer_cycles64() - start);
}

static int errors_seen = 0;
static const char* last_error = 0;
static void count_error(const char* what, void* p)
{
    errors_seen++;
    last_error = what;
}

static unsigned int seed = 12345;
static unsigned int next_random(void)
{
    seed = seed * 1103515245u + 12345u;
    return (seed >> 16) & 0x7FFF;
}

int main(void)
{
    TEST_SECTION("stress");
    unsigned char* p[48];
    unsigned int sz[48];
    for (int i = 0; i < 48; i++) p[i] = 0;
    int corrupt = 0;
    int oom = 0;
    for (int round = 0; round < 400; round++)
    {
        int i = (int)(next_random() % 48);
        if (p[i])
        {
            for (unsigned int k = 0; k < sz[i]; k++)
                if (p[i][k] != (unsigned char)(i + k)) corrupt++;
            free(p[i]);
            p[i] = 0;
        }
        else
        {
            sz[i] = 1 + next_random() % 200;
            p[i] = (unsigned char*)malloc(sz[i]);
            if (!p[i]) { oom++; continue; }
            for (unsigned int k = 0; k < sz[i]; k++) p[i][k] = (unsigned char)(i + k);
        }
        if (round % 25 == 0) CHECK_EQ(heap_check(), 0);
    }
    CHECK_EQ(corrupt, 0);
    CHECK_EQ(oom, 0);
    CHECK_EQ(heap_check(), 0);
    for (int i = 0; i < 48; i++)
        if (p[i]) { free(p[i]); p[i] = 0; }
    CHECK_EQ((int)heap_used(), 0);
    CHECK_EQ(heap_blocks(), 1);                 // everything merged back into one free block
    CHECK_EQ(heap_check(), 0);

    TEST_SECTION("edges");
    CHECK(malloc(0) == 0);
    free(0);                                    // a no-op
    void* a = malloc(1);
    CHECK(a != 0);
    CHECK_EQ((int)((unsigned int)a & 7u), 0);   // payloads are 8-byte aligned
    void* b = malloc(64);
    CHECK_EQ((int)((unsigned int)b & 7u), 0);
    CHECK(a != b);
    free(b);
    void* c = malloc(64);
    CHECK(c == b);                              // first fit reuses the block just freed
    free(c);
    free(a);
    CHECK_EQ((int)heap_used(), 0);

    TEST_SECTION("calloc");
    int* z = (int*)calloc(10, sizeof(int));
    CHECK(z != 0);
    int sum = 0;
    for (int i = 0; i < 10; i++) sum += z[i];
    CHECK_EQ(sum, 0);
    free(z);
    CHECK(calloc(0x10000, 0x10001) == 0);       // n * size overflows
    CHECK(calloc(1, 0xFFFFFFF9u) == 0);         // n * size fits, but rounding it up to 8 would wrap to 0
    CHECK(calloc(0xFFFFFFF9u, 1) == 0);

    TEST_SECTION("huge");
    // Sizes whose rounding (or header) wraps past 2^32 used to come back as a tiny block. Put a free
    // block in the heap first: a wrapped size of 0 would have been served from it.
    void* hole = malloc(32);
    void* keep = malloc(8);
    free(hole);
    CHECK(malloc(0xFFFFFFFFu) == 0);
    CHECK(malloc(0xFFFFFFFCu) == 0);
    CHECK(malloc(0xFFFFFFF9u) == 0);
    CHECK(malloc(0xFFFFFFF0u) == 0);
    CHECK(malloc(0x80000000u) == 0);            // more than any machine has
    char* grown = (char*)malloc(8);
    CHECK(grown != 0);
    CHECK(realloc(grown, 0xFFFFFFFCu) == 0);    // refused, and the block is left alone
    free(grown);
    free(keep);
    CHECK_EQ((int)heap_used(), 0);
    CHECK_EQ(heap_check(), 0);

    TEST_SECTION("reserve");
    // A reserve larger than the stack pointer leaves the heap no room: it must not wrap round to a
    // limit near 4 GiB and let the heap grow over the stack.
    heap_set_stack_reserve(0xFFFFF000u);
    CHECK(malloc(1024 * 1024) == 0);
    struct heap_stats tight;
    heap_stats(&tight);
    CHECK_EQ((int)tight.limit, 0);
    heap_set_stack_reserve(CERES_HEAP_STACK_RESERVE);   // back to the default
    void* after = malloc(1024 * 1024);
    CHECK(after != 0);
    free(after);

    TEST_SECTION("realloc");
    char* r = (char*)malloc(4);
    strcpy(r, "hi");
    r = (char*)realloc(r, 100);
    CHECK_STR(r, "hi");                         // the contents move with the block
    char* same = (char*)realloc(r, 50);
    CHECK(same == r);                           // shrinking keeps the block
    char* fresh = (char*)realloc(0, 16);        // realloc(NULL, n) is malloc(n)
    CHECK(fresh != 0);
    CHECK(realloc(fresh, 0) == 0);              // realloc(p, 0) frees
    free(r);
    CHECK_EQ((int)heap_used(), 0);

    TEST_SECTION("exhaustion");
    void* big[64];
    int got = 0;
    while (got < 64)
    {
        big[got] = malloc(1024 * 1024);
        if (!big[got]) break;
        got++;
    }
    CHECK(got > 4);                             // a 16 MiB machine has room for several MiB
    CHECK(got < 64);                            // ... and it does run out, gracefully
    CHECK(malloc(64u * 1024u * 1024u) == 0);
    for (int i = 0; i < got; i++) free(big[i]);
    CHECK_EQ((int)heap_used(), 0);
    CHECK_EQ(heap_check(), 0);
    void* again = malloc(1024 * 1024);
    CHECK(again != 0);                          // and it recovers
    free(again);

    TEST_SECTION("constant time");
    // What a malloc and a free cost with an empty heap, and again with 1000 blocks in use below: the same.
    // (A first-fit list walks every block in use on each call: 1000 blocks made it ~40 times dearer.)
    int few = pair_cost();
    static void* many[1000];
    for (int i = 0; i < 1000; i++)
        many[i] = malloc((size_t)(8 + (i * 37) % 300));
    int lots = pair_cost();
    CHECK(lots < few + few / 2);
    for (int i = 0; i < 1000; i++)
        free(many[(i * 197) % 1000]);                   // 197 is prime to 1000: every block, once, scattered
    CHECK_EQ((int)heap_used(), 0);
    CHECK_EQ(heap_check(), 0);
    CHECK_EQ(heap_blocks(), 1);                         // and they merged back into one

    TEST_SECTION("realloc in place");
    char* low = (char*)malloc(64);
    char* mid = (char*)malloc(64);
    char* top = (char*)malloc(64);
    memset(mid, 'm', 64);
    free(top);
    char* wider = (char*)realloc(mid, 100);             // the free block above is absorbed
    CHECK(wider == mid);
    CHECK(wider[63] == 'm');
    char* widest = (char*)realloc(wider, 4000);         // and the rest of the free space above
    CHECK(widest == mid);
    CHECK(widest[0] == 'm');
    CHECK(malloc_usable_size(widest) >= 4000);
    char* narrow = (char*)realloc(widest, 16);          // shrinking gives the rest back
    CHECK(narrow == mid);
    CHECK(heap_free() > 3000);
    CHECK_EQ(heap_check(), 0);
    char* blocked = (char*)malloc(8);                   // now above mid: mid cannot grow in place
    char* moved = (char*)realloc(narrow, 5000);
    CHECK(moved != mid && moved != 0);
    CHECK(moved[15] == 'm');
    free(moved);
    free(blocked);
    free(low);
    CHECK_EQ((int)heap_used(), 0);
    CHECK_EQ(heap_check(), 0);

    TEST_SECTION("aligned");
    void* a16 = aligned_alloc(16, 40);
    void* a256 = aligned_alloc(256, 10);
    void* a4k = aligned_alloc(4096, 4096);
    CHECK(a16 != 0 && a256 != 0 && a4k != 0);
    CHECK_EQ((int)((unsigned int)a16 & 15u), 0);
    CHECK_EQ((int)((unsigned int)a256 & 255u), 0);
    CHECK_EQ((int)((unsigned int)a4k & 4095u), 0);
    memset(a4k, 1, 4096);
    CHECK(malloc_usable_size(a256) >= 10);
    CHECK_EQ(heap_check(), 0);
    errno = 0;
    CHECK(aligned_alloc(24, 8) == 0);                   // not a power of two
    CHECK_EQ(errno, EINVAL);
    void* pm = 0;
    CHECK_EQ(posix_memalign(&pm, 64, 100), 0);
    CHECK_EQ((int)((unsigned int)pm & 63u), 0);
    CHECK_EQ(posix_memalign(&pm, 2, 100), EINVAL);      // below sizeof(void*)
    CHECK_EQ(posix_memalign(&pm, 0x80000000u, 8), ENOMEM);   // a valid alignment, but too large
    free(pm);
    free(a4k);
    free(a256);
    free(a16);
    CHECK_EQ((int)heap_used(), 0);
    CHECK_EQ(heap_check(), 0);

    TEST_SECTION("misuse");
    heap_set_error_handler(count_error);
    char* once = (char*)malloc(24);
    char* other = (char*)malloc(24);
    free(once);
    free(once);                                         // a double free is reported, and does nothing
    CHECK_EQ(errors_seen, 1);
    CHECK_STR(last_error, "double free");
    static char not_heap[16];
    free(not_heap + 8);
    CHECK_EQ(errors_seen, 2);
    CHECK(realloc(once, 64) == 0);                      // a freed block cannot be resized
    CHECK_EQ(errors_seen, 3);
    char* pair = (char*)malloc(24);
    memset(pair, 0, 24);
    free(pair + 8);                                     // inside a block: not a header to trust
    CHECK_EQ(errors_seen, 4);
    CHECK_EQ((int)malloc_usable_size(pair + 8), 0);
    char* lowb = (char*)malloc(24);
    char* highb = (char*)malloc(24);
    char* guard = (char*)malloc(24);
    free(lowb);
    free(highb);                                        // merged into the free block below it
    free(highb);                                        // its old header is stale now: refused
    CHECK_EQ(errors_seen, 5);
    CHECK_EQ(heap_check(), 0);
    free(guard);
    free(pair);
    CHECK_EQ(heap_check(), 0);                          // and the heap is none the worse
    free(other);
    CHECK(heap_set_error_handler(0) == count_error);   // it says what it replaces
    CHECK(heap_set_error_handler(0) != count_error);   // and 0 put the default back
    void* none = &errors_seen;
    CHECK_EQ(posix_memalign(&none, 16, 0), 0);          // a size of 0: NULL, and no error
    CHECK(none == 0);
    void* small = aligned_alloc(8, 24);                 // malloc's own alignment: a plain block
    CHECK(small != 0 && ((unsigned int)small & 7u) == 0);
    free(small);
    CHECK_EQ((int)heap_used(), 0);

    TEST_SECTION("stats");
    struct heap_stats s;
    void* m1 = malloc(100);
    void* m2 = malloc(200);
    heap_stats(&s);
    CHECK(s.used >= 300);
    CHECK(s.blocks >= 2);
    CHECK(s.brk > s.start);
    CHECK(s.limit > s.brk);
    CHECK_EQ((int)(s.used + s.free_bytes), (int)(heap_used() + heap_free()));
    free(m1);
    free(m2);
    return test_summary();
}
