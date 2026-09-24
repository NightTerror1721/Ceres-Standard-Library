// The linear arena, the fixed-block pool and the single-core atomics.
#include "ceres/test.h"
#include "ceres/arena.h"
#include "ceres/pool.h"
#include "ceres/atomic.h"
#include "ceres/heap.h"

static char raw_storage[1032];

// The arena aligns addresses, so a buffer that starts 4 bytes past an 8-byte boundary would give it less room
// than its size says - and where a static lands depends on the optimization level. Start from a known place.
#define storage ((char*)(((unsigned int)raw_storage + 7u) & ~7u))

static void arenas(void)
{
    struct arena a;

    TEST_SECTION("bump allocation");
    memset(storage, 0xFF, 1024);
    arena_init(&a, storage, 256);
    CHECK_EQ((int)arena_left(&a), 256);
    char* p1 = (char*)arena_alloc(&a, 10);
    char* p2 = (char*)arena_alloc(&a, 1);
    char* p3 = (char*)arena_alloc(&a, 24);
    CHECK(p1 != NULL && p2 != NULL && p3 != NULL);
    CHECK_EQ((int)((unsigned int)p1 & 7), 0);                  // every block starts on 8 bytes
    CHECK_EQ((int)((unsigned int)p2 & 7), 0);
    CHECK_EQ((int)((unsigned int)p3 & 7), 0);
    CHECK_EQ((int)(p2 - p1), 16);                              // 10 rounded up to the next 8
    CHECK_EQ((int)(p3 - p2), 8);
    CHECK(p1 >= storage && p3 + 24 <= storage + 256);          // and inside the buffer

    TEST_SECTION("it says no instead of running over");
    arena_reset(&a);
    CHECK(arena_alloc(&a, 256) != NULL);                       // exactly all of it
    CHECK(arena_alloc(&a, 1) == NULL);
    CHECK_EQ((int)arena_left(&a), 0);
    arena_reset(&a);
    CHECK(arena_alloc(&a, 257) == NULL);                       // bigger than the whole
    CHECK(arena_alloc(&a, 0xFFFFFFFFu) == NULL);               // and a size that would wrap the arithmetic
    CHECK_EQ((int)arena_left(&a), 256);                        // a refused request takes nothing

    TEST_SECTION("marks");
    arena_reset(&a);
    (void)arena_alloc(&a, 16);
    unsigned int mark = arena_mark(&a);
    char* t1 = (char*)arena_alloc(&a, 40);
    char* t2 = (char*)arena_alloc(&a, 8);
    CHECK(t1 != NULL && t2 != NULL);
    arena_release(&a, mark);
    CHECK_EQ((int)arena_mark(&a), (int)mark);
    CHECK(arena_alloc(&a, 40) == t1);                          // the same space, handed out again
    arena_release(&a, 0xFFFFFF);                               // a mark from the future does nothing
    CHECK(arena_mark(&a) < 100);

    TEST_SECTION("zeroed memory and strings");
    arena_reset(&a);
    memset(storage, 0xFF, 1024);
    unsigned char* z = (unsigned char*)arena_alloc_zero(&a, 30);
    int zero = 1;
    for (int i = 0; i < 30; i++) if (z[i] != 0) zero = 0;
    CHECK(zero);
    CHECK_EQ(z[30], 0xFF);                                     // and only what was asked for
    char* s = arena_strdup(&a, "hello");
    CHECK_STR(s, "hello");
    char* e = arena_strdup(&a, "");
    CHECK_STR(e, "");
    CHECK(s != e);
    arena_reset(&a);
    (void)arena_alloc(&a, 250);
    CHECK(arena_strdup(&a, "too long to fit") == NULL);

    TEST_SECTION("a buffer that is not aligned itself");
    struct arena u;
    arena_init(&u, storage + 1, 200);                          // starts one byte past a word
    char* q1 = (char*)arena_alloc(&u, 3);
    char* q2 = (char*)arena_alloc(&u, 3);
    CHECK_EQ((int)((unsigned int)q1 & 7), 0);
    CHECK_EQ((int)((unsigned int)q2 & 7), 0);
    CHECK(q1 >= storage + 1 && q2 + 3 <= storage + 201);
    char* q3 = (char*)arena_alloc(&u, 190);                    // the padding counts: this one no longer fits
    CHECK(q3 == NULL);

    TEST_SECTION("from the heap");
    unsigned int before = heap_used();
    struct arena h;
    CHECK_EQ(arena_init_heap(&h, 4096), 0);
    CHECK(heap_used() > before);
    CHECK(arena_alloc(&h, 4000) != NULL);
    CHECK(arena_alloc(&h, 200) == NULL);
    arena_destroy(&h);
    CHECK(h.base == NULL);
    CHECK_EQ((int)h.size, 0);
    arena_destroy(&h);                                         // twice is harmless
    struct arena c;
    arena_init(&c, storage, 64);
    arena_destroy(&c);                                         // memory the arena does not own is left alone
    struct arena none;
    CHECK_EQ(arena_init_heap(&none, 0x7FFFFFFF), -1);          // more than there is
    CHECK(arena_alloc(&none, 1) == NULL);
}

static char blocks[64 * 8];

static void pools(void)
{
    struct pool p;

    TEST_SECTION("blocks");
    pool_init(&p, blocks, 8, 4);
    CHECK_EQ((int)pool_used(&p), 0);
    CHECK_EQ(pool_full(&p), 0);
    void* b0 = pool_alloc(&p);
    void* b1 = pool_alloc(&p);
    void* b2 = pool_alloc(&p);
    void* b3 = pool_alloc(&p);
    CHECK(b0 != NULL && b1 != NULL && b2 != NULL && b3 != NULL);
    CHECK_EQ((int)((char*)b1 - (char*)b0), 8);
    CHECK_EQ((int)((char*)b3 - (char*)b0), 24);
    CHECK_EQ((int)pool_used(&p), 4);
    CHECK_EQ(pool_full(&p), 1);
    CHECK(pool_alloc(&p) == NULL);

    TEST_SECTION("free and reuse");
    pool_free(&p, b1);
    CHECK_EQ((int)pool_used(&p), 3);
    CHECK_EQ(pool_full(&p), 0);
    pool_free(&p, b2);
    CHECK(pool_alloc(&p) == b2);                               // the most recently freed comes back first
    CHECK(pool_alloc(&p) == b1);
    CHECK(pool_alloc(&p) == NULL);
    CHECK_EQ((int)pool_used(&p), 4);

    TEST_SECTION("what it will not free");
    pool_free(&p, NULL);
    pool_free(&p, (char*)b0 + 3);                              // inside a block but not at its start
    pool_free(&p, (char*)b0 - 8);                              // before the pool
    pool_free(&p, (char*)b0 + 32);                             // after it
    CHECK_EQ((int)pool_used(&p), 4);                           // nothing changed

    TEST_SECTION("clear");
    pool_clear(&p);
    CHECK_EQ((int)pool_used(&p), 0);
    CHECK(pool_alloc(&p) == b0);                               // from the start again
    CHECK(pool_alloc(&p) == b1);

    TEST_SECTION("blocks smaller than a pointer, and odd sizes");
    struct pool tiny;
    pool_init(&tiny, blocks, 1, 6);                            // a block is at least 4 bytes
    char* t0 = (char*)pool_alloc(&tiny);
    char* t1 = (char*)pool_alloc(&tiny);
    CHECK_EQ((int)(t1 - t0), 4);
    pool_free(&tiny, t0);
    pool_free(&tiny, t1);
    CHECK(pool_alloc(&tiny) == t1);
    struct pool odd;
    pool_init(&odd, blocks, 6, 4);                             // 6 rounds up to 8
    char* o0 = (char*)pool_alloc(&odd);
    char* o1 = (char*)pool_alloc(&odd);
    CHECK_EQ((int)(o1 - o0), 8);
    struct pool empty;
    pool_init(&empty, blocks, 8, 0);
    CHECK_EQ(pool_full(&empty), 1);
    CHECK(pool_alloc(&empty) == NULL);

    TEST_SECTION("a churn of allocations");
    struct pool many;
    pool_init(&many, blocks, 8, 64);
    void* held[64];
    for (int i = 0; i < 64; i++) held[i] = pool_alloc(&many);
    CHECK_EQ(pool_full(&many), 1);
    int distinct = 1;
    for (int i = 0; i < 64 && distinct; i++)
        for (int k = i + 1; k < 64; k++)
            if (held[i] == held[k]) { distinct = 0; break; }
    CHECK(distinct);
    for (int i = 0; i < 64; i += 2) pool_free(&many, held[i]);
    CHECK_EQ((int)pool_used(&many), 32);
    for (int i = 0; i < 32; i++) CHECK(pool_alloc(&many) != NULL);
    CHECK(pool_alloc(&many) == NULL);
}

static void atomics(void)
{
    TEST_SECTION("atomic operations");
    atomic_int n = 5;
    CHECK_EQ(atomic_load(&n), 5);
    atomic_store(&n, 9);
    CHECK_EQ(atomic_load(&n), 9);
    CHECK_EQ(atomic_add(&n, 3), 9);                            // the value before
    CHECK_EQ(atomic_load(&n), 12);
    CHECK_EQ(atomic_add(&n, -12), 12);
    CHECK_EQ(atomic_load(&n), 0);
    CHECK_EQ(atomic_swap(&n, 77), 0);
    CHECK_EQ(atomic_load(&n), 77);
    CHECK_EQ(atomic_cas(&n, 76, 1), 0);                        // it was not 76
    CHECK_EQ(atomic_load(&n), 77);
    CHECK_EQ(atomic_cas(&n, 77, 1), 1);
    CHECK_EQ(atomic_load(&n), 1);

    TEST_SECTION("critical sections");
    int inside = 0;
    {
        CRITICAL_BEGIN();
        inside = 1;
        CRITICAL_END();
    }
    CHECK_EQ(inside, 1);
    unsigned int a = irq_save();                               // nesting: the inner restore leaves them masked
    unsigned int b = irq_save();
    irq_restore(b);
    irq_restore(a);
    CHECK_EQ(irq_save() == a, 1);
    irq_restore(a);

    TEST_SECTION("critical sections with interrupts on");
    irq_enable_all();                                          // no device is armed: nothing arrives
    unsigned int on = irq_save();
    CHECK_EQ((int)on, 16);                                     // they were on ...
    CHECK_EQ((int)irq_save(), 0);                              // ... and now they are masked
    irq_restore(on);
    CHECK_EQ((int)(irq_save)(), 16);                           // the CASM function sees the same
    CHECK_EQ(atomic_add(&n, 1), 1);
    CHECK_EQ((int)irq_save(), 0);                              // (irq_save)() masked them again
}

int main(void)
{
    arenas();
    pools();
    atomics();
    return test_summary();
}
