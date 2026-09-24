// Arithmetic, pseudo-random numbers, searching, and the environment. (Text to number is in
// strtox.c, dynamic memory in malloc.c.)
#include "stdlib.h"
#include "errno.h"
#include "stdio.h"
#include "inttypes.h"
#include "ceres.h"

// ---- arithmetic ----

// The functions behind the macros in stdlib.h, for a call through a pointer.
#undef abs
#undef labs
int abs(int v)   { return v < 0 ? (int)(0u - (unsigned int)v) : v; }   // wraps for INT_MIN, on purpose
int labs(int v)  { return abs(v); }
long long llabs(long long v) { return v < 0 ? (long long)(0ULL - (unsigned long long)v) : v; }   // wraps for LLONG_MIN
intmax_t imaxabs(intmax_t v) { return llabs(v); }

div_t div(int num, int den)
{
    div_t r;
    r.quot = num / den;
    r.rem = num % den;
    return r;
}

div_t ldiv(int num, int den)  { return div(num, den); }

lldiv_t lldiv(long long num, long long den)
{
    lldiv_t r;
    r.quot = num / den;
    r.rem = num % den;
    return r;
}

imaxdiv_t imaxdiv(intmax_t num, intmax_t den)
{
    imaxdiv_t r;
    r.quot = num / den;
    r.rem = num % den;
    return r;
}

// ---- rand ----

static unsigned int rand_state = 1;

void srand(unsigned int seed) { rand_state = seed; }

int rand(void)
{
    rand_state = rand_state * 1103515245u + 12345u;
    return (int)((rand_state >> 16) & 0x7FFF);
}

// ---- searching ---- (qsort and qsort_r are in qsort.c)

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

// ---- the process ---- (exit, _Exit, abort and atexit are in exit.c; the environment in env.c)

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
