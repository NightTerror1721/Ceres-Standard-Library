// The stable sort. See ceres/sort.h.
//
// Bottom-up merge sort: runs of RUN elements are put in order by insertion (which is stable: an element
// moves left only past a strictly greater one), then runs are merged in pairs, twice as long each pass,
// back and forth between the array and a buffer. A merge takes from the left run while its element is
// not greater than the right one's - that is what keeps equal elements in order - and two runs already
// in order (the left one's last <= the right one's first) are copied in one piece.
#include "ceres/sort.h"
#include "stdlib.h"
#include "string.h"

#define RUN 8

static void copy_element(char* dst, const char* src, size_t size)
{
    if (size == 4 && (((unsigned int)dst | (unsigned int)src) & 3u) == 0)
        *(unsigned int*)dst = *(const unsigned int*)src;
    else
        memcpy(dst, src, size);
}

static void insertion_run(char* base, size_t n, size_t size, int (*cmp)(const void*, const void*), char* hold)
{
    for (size_t i = 1; i < n; i++)
    {
        if (cmp(base + (i - 1) * size, base + i * size) <= 0)
            continue;
        copy_element(hold, base + i * size, size);
        size_t j = i;
        while (j > 0 && cmp(base + (j - 1) * size, hold) > 0)
            j--;
        memmove(base + (j + 1) * size, base + j * size, (i - j) * size);
        copy_element(base + j * size, hold, size);
    }
}

// Merges src[lo, mid) and src[mid, hi) into dst[lo, hi).
static void merge(char* dst, const char* src, size_t lo, size_t mid, size_t hi, size_t size,
                  int (*cmp)(const void*, const void*))
{
    if (cmp(src + (mid - 1) * size, src + mid * size) <= 0)
    {
        memcpy(dst + lo * size, src + lo * size, (hi - lo) * size);
        return;
    }
    size_t a = lo;
    size_t b = mid;
    size_t out = lo;
    while (a < mid && b < hi)
    {
        if (cmp(src + a * size, src + b * size) <= 0)
        {
            copy_element(dst + out * size, src + a * size, size);
            a++;
        }
        else
        {
            copy_element(dst + out * size, src + b * size, size);
            b++;
        }
        out++;
    }
    if (a < mid)
        memcpy(dst + out * size, src + a * size, (mid - a) * size);
    else if (b < hi)
        memcpy(dst + out * size, src + b * size, (hi - b) * size);
}

int sort_stable(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*))
{
    if (n < 2 || size == 0)
        return 0;
    if (n > (size_t)-1 / size - 1)                        // n * size, and the element held aside
        return -1;
    char* buffer = (char*)malloc(n * size + size);
    if (buffer == 0)
        return -1;
    char* hold = buffer + n * size;

    char* array = (char*)base;
    for (size_t lo = 0; lo < n; lo += RUN)
        insertion_run(array + lo * size, n - lo < RUN ? n - lo : RUN, size, cmp, hold);

    char* src = array;
    char* dst = buffer;
    for (size_t width = RUN; width < n; width *= 2)
    {
        for (size_t lo = 0; lo < n; lo += 2 * width)
        {
            size_t mid = lo + width < n ? lo + width : n;
            size_t hi = lo + 2 * width < n ? lo + 2 * width : n;
            if (mid < hi)
                merge(dst, src, lo, mid, hi, size, cmp);
            else
                memcpy(dst + lo * size, src + lo * size, (hi - lo) * size);   // a last run with no partner
        }
        char* t = src;
        src = dst;
        dst = t;
    }
    if (src != array)
        memcpy(array, src, n * size);
    free(buffer);
    return 0;
}
