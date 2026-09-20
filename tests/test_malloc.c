// The allocator: a randomized stress run with a fill pattern, then the edges.
// Addresses are never printed - they change with the optimization level.
#include "ceres/test.h"
#include "ceres/heap.h"
#include "string.h"

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
