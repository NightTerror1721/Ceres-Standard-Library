// qsort and qsort_r: introsort. A median-of-three quicksort that recurses into the smaller half (so the
// stack stays O(log n)) and finishes runs of 12 or fewer with insertion sort - until a range has been
// split 2*log2(n) times without getting small, which only a bad or hostile input does; that range is
// then heap-sorted. Every input costs O(n log n) comparisons, where plain quicksort could be driven to
// O(n^2) by one built against it. Not stable (ceres/sort.h has a stable sort).
#include "stdlib.h"

// One sort's constant part. Exactly one of the two comparators is set: qsort's, or qsort_r's with the
// context it passes along.
struct sorter
{
    size_t size;
    int (*cmp)(const void*, const void*);
    int (*cmp_r)(const void*, const void*, void*);
    void* ctx;
};

static int compare(const struct sorter* s, const char* a, const char* b)
{
    if (s->cmp_r != 0)
        return s->cmp_r(a, b, s->ctx);
    return s->cmp(a, b);
}

// A word at a time when both are aligned and the size is whole words, with the two commonest sizes
// (an int or a pointer, and a pair of them) spelled out.
static void swap_elements(char* a, char* b, size_t n)
{
    if ((((unsigned int)a | (unsigned int)b | (unsigned int)n) & 3u) == 0)
    {
        unsigned int* x = (unsigned int*)a;
        unsigned int* y = (unsigned int*)b;
        unsigned int t = x[0];
        x[0] = y[0];
        y[0] = t;
        if (n == 4)
            return;
        t = x[1];
        x[1] = y[1];
        y[1] = t;
        for (size_t i = 2; i < n / 4; i++)
        {
            t = x[i];
            x[i] = y[i];
            y[i] = t;
        }
        return;
    }
    for (size_t i = 0; i < n; i++)
    {
        char t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

static void insertion_sort(const struct sorter* s, char* base, size_t n)
{
    size_t sz = s->size;
    for (size_t i = 1; i < n; i++)
        for (size_t j = i; j > 0 && compare(s, base + (j - 1) * sz, base + j * sz) > 0; j--)
            swap_elements(base + (j - 1) * sz, base + j * sz, sz);
}

// Moves the element at `root` down the max-heap of the first n elements until both children are smaller.
static void sift_down(const struct sorter* s, char* base, size_t root, size_t n)
{
    size_t sz = s->size;
    for (;;)
    {
        size_t child = 2 * root + 1;
        if (child >= n)
            return;
        if (child + 1 < n && compare(s, base + child * sz, base + (child + 1) * sz) < 0)
            child++;
        if (compare(s, base + root * sz, base + child * sz) >= 0)
            return;
        swap_elements(base + root * sz, base + child * sz, sz);
        root = child;
    }
}

static void heap_sort(const struct sorter* s, char* base, size_t n)
{
    size_t sz = s->size;
    for (size_t i = n / 2; i > 0; i--)
        sift_down(s, base, i - 1, n);
    for (size_t end = n - 1; end > 0; end--)
    {
        swap_elements(base, base + end * sz, sz);        // the largest goes to the end
        sift_down(s, base, 0, end);
    }
}

static void intro_sort(const struct sorter* s, char* base, size_t n, int depth)
{
    size_t sz = s->size;
    while (n > 12)
    {
        if (depth == 0)
        {
            heap_sort(s, base, n);
            return;
        }
        depth--;
        char* lo = base;
        char* mid = base + (n / 2) * sz;
        char* hi = base + (n - 1) * sz;
        if (compare(s, mid, lo) < 0) swap_elements(mid, lo, sz);
        if (compare(s, hi, lo) < 0)  swap_elements(hi, lo, sz);
        if (compare(s, hi, mid) < 0) swap_elements(hi, mid, sz);
        // lo <= mid <= hi now; the median becomes the pivot, parked next to the end, and lo and hi
        // act as sentinels for the two scans below.
        char* pivot = hi - sz;
        swap_elements(mid, pivot, sz);
        char* i = lo;
        char* j = pivot;
        for (;;)
        {
            do { i += sz; } while (compare(s, i, pivot) < 0);
            do { j -= sz; } while (compare(s, j, pivot) > 0);
            if (i >= j)
                break;
            swap_elements(i, j, sz);
        }
        swap_elements(i, pivot, sz);                    // the pivot is now in its final place
        size_t left = (size_t)(i - base) / sz;
        size_t right = n - left - 1;
        if (left < right) { intro_sort(s, base, left, depth); base = i + sz; n = right; }
        else              { intro_sort(s, i + sz, right, depth); n = left; }
    }
    insertion_sort(s, base, n);
}

static void sort(const struct sorter* s, void* base, size_t n)
{
    if (n < 2 || s->size == 0)
        return;
    int depth = 2 * (int)(31u - __builtin_clz((unsigned int)n));   // 2 * floor(log2 n)
    intro_sort(s, (char*)base, n, depth);
}

void qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*))
{
    struct sorter s;
    s.size = size;
    s.cmp = cmp;
    s.cmp_r = 0;
    s.ctx = 0;
    sort(&s, base, n);
}

void qsort_r(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*, void*), void* ctx)
{
    struct sorter s;
    s.size = size;
    s.cmp = 0;
    s.cmp_r = cmp;
    s.ctx = ctx;
    sort(&s, base, n);
}
