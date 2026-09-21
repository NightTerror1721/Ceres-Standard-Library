// Arithmetic, pseudo-random numbers, sorting and searching, and the environment. (Text to number is in
// strtox.c, dynamic memory in malloc.c.)
#include "stdlib.h"
#include "errno.h"
#include "stdio.h"
#include "ceres.h"

// ---- arithmetic ----

int abs(int v)   { return v < 0 ? (int)(0u - (unsigned int)v) : v; }   // wraps for INT_MIN, on purpose
int labs(int v)  { return abs(v); }
int llabs(int v) { return abs(v); }
int imaxabs(int v) { return abs(v); }

div_t div(int num, int den)
{
    div_t r;
    r.quot = num / den;
    r.rem = num % den;
    return r;
}

div_t ldiv(int num, int den)  { return div(num, den); }
div_t lldiv(int num, int den) { return div(num, den); }

// ---- rand ----

static unsigned int rand_state = 1;

void srand(unsigned int seed) { rand_state = seed; }

int rand(void)
{
    rand_state = rand_state * 1103515245u + 12345u;
    return (int)((rand_state >> 16) & 0x7FFF);
}

// ---- qsort ----
// Median-of-three quicksort that recurses into the smaller half (so the stack is O(log n)) and
// finishes runs of 12 or fewer with insertion sort.

static void swap_elements(char* a, char* b, size_t n)
{
    if ((((unsigned int)a | (unsigned int)b | (unsigned int)n) & 3u) == 0)
    {
        // aligned and a multiple of 4 bytes: move a word at a time
        unsigned int* x = (unsigned int*)a;
        unsigned int* y = (unsigned int*)b;
        for (size_t i = 0; i < n / 4; i++)
        {
            unsigned int t = x[i];
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

static void insertion_sort(char* base, size_t n, size_t sz, int (*cmp)(const void*, const void*))
{
    for (size_t i = 1; i < n; i++)
        for (size_t j = i; j > 0 && cmp(base + (j - 1) * sz, base + j * sz) > 0; j--)
            swap_elements(base + (j - 1) * sz, base + j * sz, sz);
}

static void quick_sort(char* base, size_t n, size_t sz, int (*cmp)(const void*, const void*))
{
    while (n > 12)
    {
        char* lo = base;
        char* mid = base + (n / 2) * sz;
        char* hi = base + (n - 1) * sz;
        if (cmp(mid, lo) < 0) swap_elements(mid, lo, sz);
        if (cmp(hi, lo) < 0)  swap_elements(hi, lo, sz);
        if (cmp(hi, mid) < 0) swap_elements(hi, mid, sz);
        // lo <= mid <= hi now; the median becomes the pivot, parked next to the end, and lo and hi
        // act as sentinels for the two scans below.
        char* pivot = hi - sz;
        swap_elements(mid, pivot, sz);
        char* i = lo;
        char* j = pivot;
        for (;;)
        {
            do { i += sz; } while (cmp(i, pivot) < 0);
            do { j -= sz; } while (cmp(j, pivot) > 0);
            if (i >= j)
                break;
            swap_elements(i, j, sz);
        }
        swap_elements(i, pivot, sz);                    // the pivot is now in its final place
        size_t left = (size_t)(i - base) / sz;
        size_t right = n - left - 1;
        if (left < right) { quick_sort(base, left, sz, cmp); base = i + sz; n = right; }
        else              { quick_sort(i + sz, right, sz, cmp); n = left; }
    }
    insertion_sort(base, n, sz, cmp);
}

void qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*))
{
    if (n < 2 || size == 0)
        return;
    quick_sort((char*)base, n, size, cmp);
}

void* bsearch(const void* key, const void* base, size_t n, size_t size, int (*cmp)(const void*, const void*))
{
    size_t lo = 0;
    size_t hi = n;
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        const char* item = (const char*)base + mid * size;
        int c = cmp(key, item);
        if (c == 0)
            return (void*)item;
        if (c < 0) hi = mid;
        else       lo = mid + 1;
    }
    return 0;
}

// ---- the process ---- (exit, _Exit, abort and atexit are in exit.c)

char* getenv(const char* name)
{
    return 0;
}

int system(const char* cmd)
{
    if (cmd == 0)
        return 0;                          // "is there a shell?" - no
    errno = ENOSYS;
    return -1;
}

// ---- itoa / utoa ----

char* utoa(unsigned int v, char* buf, int base)
{
    char tmp[33];                          // 32 binary digits and a spare
    int n = 0;
    if (base < 2 || base > 36)
        base = 10;
    if (v == 0)
    {
        tmp[0] = '0';
        n = 1;
    }
    while (v != 0)
    {
        int d = (int)(v % (unsigned int)base);
        tmp[n] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
        n++;
        v /= (unsigned int)base;
    }
    for (int i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];
    buf[n] = 0;
    return buf;
}

char* itoa(int v, char* buf, int base)
{
    if (v < 0 && base == 10)
    {
        buf[0] = '-';
        utoa(0u - (unsigned int)v, buf + 1, 10);
        return buf;
    }
    return utoa((unsigned int)v, buf, base);
}

// Fixed-point text of a float, `decimals` digits after the point (like "%.*f"). Needs a buffer of
// sign + integer digits + point + decimals + NUL; a float below 1e10 with 9 decimals fits 32 bytes.
char* ftoa(float v, char* buf, int decimals)
{
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;
    sprintf(buf, "%.*f", decimals, v);
    return buf;
}
