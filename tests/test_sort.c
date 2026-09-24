// qsort and qsort_r (the introsort in src/qsort.c) and the stable sort (ceres/sort.h, and the vector's):
// every shape and element size ends up in order; an adversary that builds the worst input for any
// quicksort while it runs (McIlroy's "antiqsort") costs O(n log n) comparisons, not O(n^2); qsort_r
// hands its context through; the stable sort keeps equal elements in their order.
#include "ceres/test.h"
#include "ceres/sort.h"
#include "ceres/ds/vector.h"
#include "stdlib.h"
#include "string.h"

#define N 600

static int values[N];
static unsigned int comparisons;
static int copy[N];
static unsigned int seed = 12345;

static int next_random(void)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 8) & 0xFFFF);
}

static int cmp_int(const void* a, const void* b)
{
    int x = *(const int*)a;
    int y = *(const int*)b;
    return (x > y) - (x < y);
}

static int cmp_int_counted(const void* a, const void* b)
{
    comparisons++;
    return cmp_int(a, b);
}

static int sorted(const int* v, int n)
{
    for (int i = 1; i < n; i++)
        if (v[i - 1] > v[i]) return 0;
    return 1;
}

// The same multiset: the sum and the sum of squares survive any permutation and hardly anything else.
static int same_values(const int* a, const int* b, int n)
{
    unsigned int s1 = 0, s2 = 0, q1 = 0, q2 = 0;
    for (int i = 0; i < n; i++)
    {
        s1 += (unsigned int)a[i]; q1 += (unsigned int)(a[i] * a[i]);
        s2 += (unsigned int)b[i]; q2 += (unsigned int)(b[i] * b[i]);
    }
    return s1 == s2 && q1 == q2;
}

// shape 0 random, 1 sorted, 2 reversed, 3 all equal, 4 organ pipe, 5 few distinct values, 6 sawtooth
static void make(int shape, int n)
{
    for (int i = 0; i < n; i++)
    {
        int v = 0;
        if (shape == 0) v = next_random();
        if (shape == 1) v = i;
        if (shape == 2) v = n - i;
        if (shape == 3) v = 7;
        if (shape == 4) v = i < n / 2 ? i : n - i;
        if (shape == 5) v = next_random() % 4;
        if (shape == 6) v = i % 16;
        values[i] = v;
        copy[i] = v;
    }
}

static void shapes(void)
{
    TEST_SECTION("qsort: every shape and length");
    int ok = 1;
    for (int shape = 0; shape < 7; shape++)
        for (int n = 0; n <= 70; n += (n < 20 ? 1 : 7))
        {
            make(shape, n);
            qsort(values, (size_t)n, sizeof(int), cmp_int);
            if (!sorted(values, n) || !same_values(values, copy, n)) ok = 0;
        }
    make(0, N);
    qsort(values, N, sizeof(int), cmp_int);
    if (!sorted(values, N) || !same_values(values, copy, N)) ok = 0;
    CHECK(ok);
}

// Elements of 1, 2, 3, 8 and 12 bytes, some at an odd address: the swaps for every size and alignment.
struct wide { int key; int a; int b; };
static int cmp_wide(const void* x, const void* y) { return cmp_int(x, y); }
static int cmp_byte(const void* x, const void* y) { return (int)*(const unsigned char*)x - (int)*(const unsigned char*)y; }
static int cmp_half(const void* x, const void* y)
{
    unsigned short p, q;
    memcpy(&p, x, 2);
    memcpy(&q, y, 2);
    return (int)p - (int)q;
}
static int cmp_three(const void* x, const void* y) { return memcmp(x, y, 3); }
static int cmp_pair(const void* x, const void* y)
{
    long long p, q;
    memcpy(&p, x, 8);
    memcpy(&q, y, 8);
    return (p > q) - (p < q);
}

static unsigned char raw[N * 12 + 4];

static void element_sizes(void)
{
    TEST_SECTION("qsort: element sizes and alignments");
    int ok = 1;
    for (int i = 0; i < 200; i++) raw[1 + i] = (unsigned char)next_random();
    qsort(raw + 1, 200, 1, cmp_byte);
    for (int i = 1; i < 200; i++) if (raw[i] > raw[1 + i]) ok = 0;

    for (int i = 0; i < 400; i++) raw[1 + i] = (unsigned char)next_random();
    qsort(raw + 1, 200, 2, cmp_half);                            // an odd address: bytes are swapped
    for (int i = 1; i < 200; i++) if (cmp_half(raw + 1 + (i - 1) * 2, raw + 1 + i * 2) > 0) ok = 0;

    for (int i = 0; i < 300; i++) raw[i] = (unsigned char)next_random();
    qsort(raw, 100, 3, cmp_three);
    for (int i = 1; i < 100; i++) if (memcmp(raw + (i - 1) * 3, raw + i * 3, 3) > 0) ok = 0;

    long long pairs[100];
    for (int i = 0; i < 100; i++) pairs[i] = ((long long)next_random() << 20) - 5000000000LL;
    qsort(pairs, 100, 8, cmp_pair);
    for (int i = 1; i < 100; i++) if (pairs[i - 1] > pairs[i]) ok = 0;

    struct wide w[150];
    for (int i = 0; i < 150; i++) { w[i].key = next_random() % 50; w[i].a = i; w[i].b = -i; }
    qsort(w, 150, sizeof(struct wide), cmp_wide);
    for (int i = 1; i < 150; i++) if (w[i - 1].key > w[i].key) ok = 0;
    for (int i = 0; i < 150; i++) if (w[i].b != -w[i].a) ok = 0;   // the words of an element stay together
    CHECK(ok);
}

// McIlroy, "A Killer Adversary for Quicksort" (1999): values are decided while the sort runs. Every
// element starts as "gas"; when two gas elements meet, one is frozen as the next smallest value, which is
// exactly what makes a quicksort's pivots as bad as they can be. The comparisons it answers are consistent,
// so the sort still ends in order.
static int adversary_value[N];
static int adversary_gas;
static int adversary_solid;
static int adversary_candidate;

static int cmp_adversary(const void* a, const void* b)
{
    int x = *(const int*)a;
    int y = *(const int*)b;
    comparisons++;
    if (adversary_value[x] == adversary_gas && adversary_value[y] == adversary_gas)
    {
        if (x == adversary_candidate) adversary_value[x] = adversary_solid++;
        else                          adversary_value[y] = adversary_solid++;
    }
    if (adversary_value[x] == adversary_gas)      adversary_candidate = x;
    else if (adversary_value[y] == adversary_gas) adversary_candidate = y;
    return adversary_value[x] - adversary_value[y];
}

static void adversary(void)
{
    TEST_SECTION("qsort: a worst case built against it stays O(n log n)");
    int n = N;
    adversary_gas = n - 1;
    adversary_solid = 0;
    adversary_candidate = 0;
    comparisons = 0;
    for (int i = 0; i < n; i++) { values[i] = i; adversary_value[i] = adversary_gas; }
    qsort(values, (size_t)n, sizeof(int), cmp_adversary);
    int ok = 1;
    for (int i = 1; i < n; i++)
        if (adversary_value[values[i - 1]] > adversary_value[values[i]]) ok = 0;
    CHECK(ok);
    // n log2 n is about 5500 here; a quicksort that this adversary beats needs n^2/4 = 90000 or more.
    CHECK(comparisons < 4u * 5600u);
}

// qsort_r: the order comes from a table the comparator reaches through the context.
static int cmp_by_table(const void* a, const void* b, void* ctx)
{
    const int* table = (const int*)ctx;
    return cmp_int(&table[*(const int*)a], &table[*(const int*)b]);
}

static void with_context(void)
{
    TEST_SECTION("qsort_r: the comparator gets the context");
    static int table[100];
    int index[100];
    for (int i = 0; i < 100; i++) { table[i] = next_random(); index[i] = i; }
    qsort_r(index, 100, sizeof(int), cmp_by_table, table);
    int ok = 1;
    for (int i = 1; i < 100; i++) if (table[index[i - 1]] > table[index[i]]) ok = 0;
    CHECK(ok);

    struct vector v;
    vector_init(&v, sizeof(int));
    for (int i = 0; i < 100; i++) vector_push_copy(&v, &i);
    vector_sort_r(&v, cmp_by_table, table);
    ok = 1;
    for (int i = 1; i < 100; i++) if (table[VECTOR_GET(&v, int, i - 1)] > table[VECTOR_GET(&v, int, i)]) ok = 0;
    CHECK(ok);
    vector_free(&v);
}

// The stable sort: records sorted by key alone keep the order of their serial numbers among equal keys.
struct record { int key; int serial; };
static int cmp_record(const void* a, const void* b) { return cmp_int(a, b); }

static int stable_in_order(const struct record* r, int n)
{
    for (int i = 1; i < n; i++)
    {
        if (r[i - 1].key > r[i].key) return 0;
        if (r[i - 1].key == r[i].key && r[i - 1].serial > r[i].serial) return 0;
    }
    return 1;
}

static struct record records[N];

static void stable(void)
{
    TEST_SECTION("sort_stable: in order, and equal keys keep their order");
    int ok = 1;
    for (int shape = 0; shape < 7; shape++)
        for (int n = 0; n <= 200; n += (n < 20 ? 1 : 23))
        {
            make(shape, n);
            for (int i = 0; i < n; i++) { records[i].key = values[i] % 10; records[i].serial = i; }
            if (sort_stable(records, (size_t)n, sizeof(struct record), cmp_record) != 0) ok = 0;
            if (!stable_in_order(records, n)) ok = 0;
        }
    CHECK(ok);

    ok = 1;
    for (int i = 0; i < 300; i++) raw[1 + i] = (unsigned char)(next_random() % 5);
    CHECK_EQ(sort_stable(raw + 1, 300, 1, cmp_byte), 0);      // one-byte elements at an odd address
    for (int i = 1; i < 300; i++) if (raw[i] > raw[1 + i]) ok = 0;
    CHECK(ok);

    make(1, N);                                               // already in order: about one comparison per element
    comparisons = 0;
    CHECK_EQ(sort_stable(values, N, sizeof(int), cmp_int_counted), 0);
    CHECK(sorted(values, N));
    CHECK(comparisons < 2u * N);

    CHECK_EQ(sort_stable(records, 0x20000000u, 16, cmp_record), -1);   // n * size does not fit: untouched
    CHECK_EQ(sort_stable(records, 1, sizeof(struct record), cmp_record), 0);

    struct vector v;
    vector_init(&v, sizeof(struct record));
    for (int i = 0; i < 150; i++)
    {
        struct record r;
        r.key = next_random() % 7;
        r.serial = i;
        vector_push_copy(&v, &r);
    }
    CHECK_EQ(vector_sort_stable(&v, cmp_record), 0);
    CHECK(stable_in_order((const struct record*)v.data, 150));
    vector_free(&v);
}

int main(void)
{
    shapes();
    element_sizes();
    adversary();
    with_context();
    stable();
    return test_summary();
}
