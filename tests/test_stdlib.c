// <stdlib.h>: arithmetic, rand, qsort, bsearch, itoa/utoa, and the process functions.
#include "ceres/test.h"
#include "stdlib.h"
#include "limits.h"
#include "errno.h"
#include "inttypes.h"
#include "ceres/heap.h"      // declares malloc too: the two headers must agree

// ---- comparators ----
static int cmp_int(const void* a, const void* b)
{
    int x = *(const int*)a;
    int y = *(const int*)b;
    return (x > y) - (x < y);
}

static int cmp_char(const void* a, const void* b) { return *(const unsigned char*)a - *(const unsigned char*)b; }

struct Odd { unsigned char key; unsigned char pad[2]; };            // 3 bytes: never word-aligned
struct Rec { int key; int tag; };                                   // 8 bytes: swapped by words
static int cmp_odd(const void* a, const void* b) { return ((const struct Odd*)a)->key - ((const struct Odd*)b)->key; }
static int cmp_rec(const void* a, const void* b) { return cmp_int(&((const struct Rec*)a)->key, &((const struct Rec*)b)->key); }
static int cmp_str(const void* a, const void* b) { return strcmp(*(const char* const*)a, *(const char* const*)b); }

// ---- helpers ----
static int is_sorted(const int* v, int n)
{
    for (int i = 1; i < n; i++)
        if (v[i - 1] > v[i]) return 0;
    return 1;
}

static void checksums(const int* v, int n, unsigned int* sum, unsigned int* mix)
{
    *sum = 0;
    *mix = 0;
    for (int i = 0; i < n; i++)
    {
        *sum += (unsigned int)v[i];
        *mix += (unsigned int)v[i] * (unsigned int)v[i] * 2654435761u;   // catches a swapped-for-copied element
    }
}

// Sorts v[0..n) with a canary on each side and checks: sorted, the same multiset, canaries intact.
static int sort_and_verify(int* v, int n)
{
    unsigned int sum0, mix0, sum1, mix1;
    int* buf = (int*)malloc((size_t)(n + 2) * sizeof(int));
    if (buf == 0) return 0;
    buf[0] = 0x1234ABCD;
    buf[n + 1] = 0x5678EF01;
    for (int i = 0; i < n; i++) buf[i + 1] = v[i];
    checksums(v, n, &sum0, &mix0);
    qsort(buf + 1, (size_t)n, sizeof(int), cmp_int);
    checksums(buf + 1, n, &sum1, &mix1);
    int ok = is_sorted(buf + 1, n) && sum0 == sum1 && mix0 == mix1 && buf[0] == 0x1234ABCD && buf[n + 1] == 0x5678EF01;
    for (int i = 0; i < n; i++) v[i] = buf[i + 1];
    free(buf);
    return ok;
}

static void fill_random(int* v, int n, unsigned int seed, int range)
{
    srand(seed);
    for (int i = 0; i < n; i++) v[i] = rand() % range - range / 2;
}

// ---- atexit ----
static void first(void)  { putstr("atexit: first registered, runs last\n"); }
static void second(void) { putstr("atexit: second registered, runs first\n"); }
static void nothing(void) {}

static int data[1200];

int main(void)
{
    TEST_SECTION("abs and div");
    CHECK_EQ(abs(5), 5);
    CHECK_EQ(abs(-5), 5);
    CHECK_EQ(abs(0), 0);
    CHECK_EQ(abs(INT_MAX), INT_MAX);
    CHECK_EQ(abs(-INT_MAX), INT_MAX);
    CHECK_EQ(abs(INT_MIN), INT_MIN);               // no positive counterpart: it wraps
    CHECK_EQ(labs(-7), 7);
    CHECK_EQ(llabs(-7), 7);
    CHECK_EQ(imaxabs(-7), 7);
    div_t d = div(17, 5);
    CHECK(d.quot == 3 && d.rem == 2);
    d = div(-17, 5);
    CHECK(d.quot == -3 && d.rem == -2);            // truncates toward zero
    d = div(17, -5);
    CHECK(d.quot == -3 && d.rem == 2);
    d = div(-17, -5);
    CHECK(d.quot == 3 && d.rem == -2);
    d = div(0, 9);
    CHECK(d.quot == 0 && d.rem == 0);
    d = div(INT_MIN, 1);
    CHECK(d.quot == INT_MIN && d.rem == 0);
    d = ldiv(-9, 4);
    CHECK(d.quot == -2 && d.rem == -1);
    lldiv_t ld = lldiv(9, 4);
    CHECK(ld.quot == 2 && ld.rem == 1);
    CHECK(RAND_MAX == 32767 && EXIT_SUCCESS == 0 && EXIT_FAILURE == 1);

    TEST_SECTION("rand");
    srand(1);
    CHECK_EQ(rand(), 16838);                       // the classic sequence for seed 1
    CHECK_EQ(rand(), 5758);
    CHECK_EQ(rand(), 10113);
    CHECK_EQ(rand(), 17515);
    CHECK_EQ(rand(), 31051);
    srand(12345);
    int a1 = rand(), a2 = rand(), a3 = rand();
    srand(12345);
    CHECK(rand() == a1 && rand() == a2 && rand() == a3);   // same seed, same sequence
    srand(54321);
    CHECK(rand() != a1 || rand() != a2);                    // another seed, another one
    int lo = RAND_MAX, hi = 0, odd = 0;
    srand(99);
    for (int i = 0; i < 2000; i++)
    {
        int r = rand();
        if (r < lo) lo = r;
        if (r > hi) hi = r;
        odd += r & 1;
    }
    CHECK(lo >= 0 && hi <= RAND_MAX);
    CHECK(lo < 500 && hi > RAND_MAX - 500);        // it covers the range
    CHECK(odd > 900 && odd < 1100);                // and the low bit is not stuck

    TEST_SECTION("qsort: sizes and shapes");
    int tiny[3] = { 3, 1, 2 };
    qsort(tiny, 0, sizeof(int), cmp_int);
    CHECK(tiny[0] == 3 && tiny[1] == 1 && tiny[2] == 2);   // n = 0 touches nothing
    qsort(tiny, 1, sizeof(int), cmp_int);
    CHECK(tiny[0] == 3);
    qsort(tiny, 2, sizeof(int), cmp_int);
    CHECK(tiny[0] == 1 && tiny[1] == 3 && tiny[2] == 2);
    qsort(tiny, 3, sizeof(int), cmp_int);
    CHECK(tiny[0] == 1 && tiny[1] == 2 && tiny[2] == 3);
    int sizes[9] = { 4, 5, 11, 12, 13, 14, 25, 100, 1000 };
    for (int s = 0; s < 9; s++)
    {
        fill_random(data, sizes[s], (unsigned int)(100 + s), 2000);
        CHECK(sort_and_verify(data, sizes[s]));
    }
    for (int i = 0; i < 1000; i++) data[i] = i;
    CHECK(sort_and_verify(data, 1000));                    // already sorted
    for (int i = 0; i < 1000; i++) data[i] = 1000 - i;
    CHECK(sort_and_verify(data, 1000));                    // reversed
    for (int i = 0; i < 300; i++) data[i] = 7;
    CHECK(sort_and_verify(data, 300));                     // all equal
    for (int i = 0; i < 400; i++) data[i] = i < 200 ? i : 400 - i;
    CHECK(sort_and_verify(data, 400));                     // organ pipe
    fill_random(data, 500, 5, 4);
    CHECK(sort_and_verify(data, 500));                     // four distinct values
    // Values at both ends of the int range, mixed: the comparator sees pairs like INT_MAX vs INT_MIN whose
    // difference overflows (the VM's CMP once got those wrong).
    for (int i = 0; i < 100; i++) data[i] = (i % 2) ? INT_MAX - i : INT_MIN + i;
    CHECK(sort_and_verify(data, 100));
    CHECK_EQ(data[0], INT_MIN);
    CHECK_EQ(data[49], INT_MIN + 98);
    CHECK_EQ(data[50], INT_MAX - 99);
    CHECK_EQ(data[99], INT_MAX - 1);
    for (int i = 0; i < 100; i++) data[i] = INT_MAX - (i * 37) % 100;
    CHECK(sort_and_verify(data, 100));
    CHECK_EQ(data[0], INT_MAX - 99);
    CHECK_EQ(data[99], INT_MAX);
    for (int i = 0; i < 100; i++) data[i] = INT_MIN + (i * 37) % 100;
    CHECK(sort_and_verify(data, 100));
    CHECK_EQ(data[0], INT_MIN);
    CHECK_EQ(data[99], INT_MIN + 99);

    TEST_SECTION("qsort: element sizes");
    unsigned char bytes[64];
    srand(3);
    for (int i = 0; i < 64; i++) bytes[i] = (unsigned char)rand();
    qsort(bytes, 64, 1, cmp_char);
    int bytes_ok = 1;
    for (int i = 1; i < 64; i++) if (bytes[i - 1] > bytes[i]) bytes_ok = 0;
    CHECK(bytes_ok);

    struct Odd odds[40];
    srand(4);
    for (int i = 0; i < 40; i++) { odds[i].key = (unsigned char)(rand() % 50); odds[i].pad[0] = odds[i].key; odds[i].pad[1] = (unsigned char)(~odds[i].key); }
    qsort(odds, 40, sizeof(struct Odd), cmp_odd);
    int odd_ok = 1;
    for (int i = 0; i < 40; i++)
    {
        if (i > 0 && odds[i - 1].key > odds[i].key) odd_ok = 0;
        if (odds[i].pad[0] != odds[i].key || odds[i].pad[1] != (unsigned char)(~odds[i].key)) odd_ok = 0;   // the payload travelled with its key
    }
    CHECK(odd_ok);

    struct Rec recs[50];
    srand(6);
    for (int i = 0; i < 50; i++) { recs[i].key = rand() % 30; recs[i].tag = recs[i].key * 1000 + 7; }
    qsort(recs, 50, sizeof(struct Rec), cmp_rec);
    int rec_ok = 1;
    for (int i = 0; i < 50; i++)
    {
        if (i > 0 && recs[i - 1].key > recs[i].key) rec_ok = 0;
        if (recs[i].tag != recs[i].key * 1000 + 7) rec_ok = 0;
    }
    CHECK(rec_ok);

    const char* words[14] = { "pear", "apple", "fig", "banana", "cherry", "date", "elderberry", "grape", "kiwi", "lemon", "mango", "nectarine", "orange", "papaya" };
    qsort(words, 14, sizeof(const char*), cmp_str);
    CHECK_STR(words[0], "apple");
    CHECK_STR(words[1], "banana");
    CHECK_STR(words[6], "grape");
    CHECK_STR(words[13], "pear");

    TEST_SECTION("bsearch");
    int sorted[20];
    for (int i = 0; i < 20; i++) sorted[i] = i * 3;        // 0, 3, ... 57
    int key = 27;
    int* hit = (int*)bsearch(&key, sorted, 20, sizeof(int), cmp_int);
    CHECK(hit == &sorted[9]);
    key = 0;
    hit = (int*)bsearch(&key, sorted, 20, sizeof(int), cmp_int);
    CHECK(hit == &sorted[0]);                              // the first
    key = 57;
    hit = (int*)bsearch(&key, sorted, 20, sizeof(int), cmp_int);
    CHECK(hit == &sorted[19]);                             // the last
    key = 28;
    CHECK(bsearch(&key, sorted, 20, sizeof(int), cmp_int) == 0);   // between two
    key = -1;
    CHECK(bsearch(&key, sorted, 20, sizeof(int), cmp_int) == 0);   // below all
    key = 100;
    CHECK(bsearch(&key, sorted, 20, sizeof(int), cmp_int) == 0);   // above all
    CHECK(bsearch(&key, sorted, 0, sizeof(int), cmp_int) == 0);    // empty
    key = 3;
    CHECK(bsearch(&key, sorted, 1, sizeof(int), cmp_int) == 0);
    key = 0;
    CHECK(bsearch(&key, sorted, 1, sizeof(int), cmp_int) == &sorted[0]);
    for (int i = 0; i < 20; i++)
    {
        key = i * 3;
        if (bsearch(&key, sorted, 20, sizeof(int), cmp_int) != &sorted[i]) { CHECK(0); }
    }

    TEST_SECTION("itoa and utoa");
    char buf[40];
    CHECK_STR(itoa(0, buf, 10), "0");
    CHECK_STR(itoa(42, buf, 10), "42");
    CHECK_STR(itoa(-42, buf, 10), "-42");
    CHECK_STR(itoa(INT_MAX, buf, 10), "2147483647");
    CHECK_STR(itoa(INT_MIN, buf, 10), "-2147483648");
    CHECK_STR(itoa(255, buf, 16), "ff");
    CHECK_STR(itoa(255, buf, 2), "11111111");
    CHECK_STR(itoa(255, buf, 8), "377");
    CHECK_STR(itoa(35, buf, 36), "z");
    CHECK_STR(itoa(-1, buf, 16), "ffffffff");            // only base 10 is signed
    CHECK_STR(utoa(4294967295u, buf, 10), "4294967295");
    CHECK_STR(utoa(4294967295u, buf, 2), "11111111111111111111111111111111");
    CHECK_STR(utoa(0, buf, 2), "0");
    CHECK_STR(utoa(123456789u, buf, 36), "21i3v9");
    CHECK_STR(utoa(10, buf, 1), "10");                   // a bad base falls back to 10
    CHECK_STR(utoa(10, buf, 37), "10");
    CHECK(itoa(7, buf, 10) == buf);                      // returns its buffer

    TEST_SECTION("the process");
    CHECK(getenv("PATH") == 0);
    CHECK_EQ(system(0), 0);
    errno = 0;
    CHECK_EQ(system("dir"), -1);
    CHECK_EQ(errno, ENOSYS);
    CHECK_EQ(atexit(first), 0);
    CHECK_EQ(atexit(second), 0);
    for (int i = 0; i < 30; i++) CHECK_EQ(atexit(nothing), 0);
    CHECK_EQ(atexit(nothing), -1);                       // 32 slots, all taken
    CHECK_EQ(atexit(0), -1);

    TEST_SECTION("exit");
    int verdict = test_summary();
    exit(verdict + 3);                                   // runs the handlers, last registered first
    putstr("NOT REACHED\n");
    return verdict;
}
