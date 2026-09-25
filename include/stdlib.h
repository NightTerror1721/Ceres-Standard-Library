#pragma once

#include "stddef.h"

// General utilities. `long` is 32 bits (`labs`/`ldiv` are the int versions), but `long long` is a
// real 64-bit type, so `llabs`/`lldiv` and `strtoll`/`strtoull`/`atoll` are 64-bit for real.

#define EXIT_SUCCESS  0
#define EXIT_FAILURE  1
#define RAND_MAX      32767
#define MB_CUR_MAX    4               // the multibyte strings are UTF-8 (ceres/utf8.h)

struct __div_s { int quot; int rem; };
typedef struct __div_s div_t;
typedef struct __div_s ldiv_t;
struct __lldiv_s { long long quot; long long rem; };
typedef struct __lldiv_s lldiv_t;

// ---- dynamic memory (src/malloc.c; the diagnostics are in ceres/heap.h) ----
void* malloc(size_t n);
void* calloc(size_t n, size_t size);
void* realloc(void* p, size_t n);
void  free(void* p);
void* aligned_alloc(size_t alignment, size_t n);            // alignment a power of two
int   posix_memalign(void** out, size_t alignment, size_t n); // 0, EINVAL or ENOMEM

// ---- text to number (src/strtox.c) ----
// Leading white space is skipped; `end`, when not NULL, receives the first character that was not
// part of the number (or `s` itself when there was no number). Overflow sets errno = ERANGE and
// returns the nearest limit. A base of 0 means "look at the prefix": 0x is 16, a bare 0 is 8.
int          atoi(const char* s);
int          atol(const char* s);
long long    atoll(const char* s);
int          strtol(const char* s, char** end, int base);
unsigned int strtoul(const char* s, char** end, int base);
long long    strtoll(const char* s, char** end, int base);
unsigned long long strtoull(const char* s, char** end, int base);
float        strtof(const char* s, char** end);    // correctly rounded; decimal, 0x hex floats ("0x1.8p3"), inf/infinity and nan
int          strfromf(char* restrict s, size_t n, const char* restrict format, float fp);   // C23: format is "%[.p]{aAeEfFgG}"
int          ftoa_shortest(char* buf, size_t size, float x);   // the fewest digits that read back as x ("0.1", "1e+30"); its length
#ifdef __CERES_SOFT_DOUBLE__
// -fsoft-double: a real double, read the way strtof reads a float and rounded once to binary64.
double       strtod(const char* s, char** end);
double       atof(const char* s);
#define strtold   strtod
#else
float        strtod(const char* s, char** end);    // == strtof (double is float)
float        atof(const char* s);
#define strtold   strtof
#endif

// ---- multibyte (UTF-8) and wide characters (src/wchar.c; more in wchar.h and uchar.h) ----
// wchar_t holds a code point. mblen/mbtowc return the bytes of the character at s (0 for the NUL, -1 with EILSEQ
// when the n bytes are not one whole character); with a NULL s they return 0, as UTF-8 has no shift states.
int    mblen(const char* s, size_t n);
int    mbtowc(wchar_t* pwc, const char* s, size_t n);
int    wctomb(char* s, wchar_t wc);                          // at most MB_CUR_MAX bytes; -1 for a non-character
size_t mbstowcs(wchar_t* dst, const char* src, size_t n);   // as mbsrtowcs (wchar.h); (size_t)-1 when src is not UTF-8
size_t wcstombs(char* dst, const wchar_t* src, size_t n);

// ---- pseudo-random numbers: a 32-bit linear congruential generator ----
// The same seed always gives the same sequence. rand() returns 0..RAND_MAX; the low bits of an LCG are
// poor, so it hands out bits 30..16. Better generators are in ceres/rand.h.
int   rand(void);
void  srand(unsigned int seed);

// ---- arithmetic ----
int   abs(int v);              // abs(INT_MIN) is INT_MIN: the negation wraps
int   labs(int v);
// One instruction. For INT_MIN the `abs` instruction gives INT_MIN back (and sets Overflow), as the
// function does; Ceres-C does not fold its builtins, so no optimization level assumes otherwise.
#define abs(v)   __builtin_abs((int)(v))
#define labs(v)  __builtin_abs((int)(v))
long long llabs(long long v);
div_t div(int num, int den);   // quotient truncated toward zero; a zero divisor sets the Trap flag
div_t ldiv(int num, int den);
lldiv_t lldiv(long long num, long long den);

// ---- searching and sorting ----
// qsort is an introsort: O(n log n) comparisons whatever the input, but not stable (ceres/sort.h has a
// stable sort). Elements of any size; the comparator receives pointers to two elements. qsort_r
// passes `ctx` to the comparator as its third argument (the POSIX and glibc form).
void  qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));
void  qsort_r(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*, void*), void* ctx);
void* bsearch(const void* key, const void* base, size_t n, size_t size,
              int (*cmp)(const void*, const void*));

// ---- the process ----
// The status is the exit status of `ceres run` (its low eight bits), and returning n from main is exit(n).
void  exit(int status) __attribute__((__noreturn__));    // runs the atexit functions (last registered first), then stops
void  _Exit(int status) __attribute__((__noreturn__));   // stops at once
void  abort(void) __attribute__((__noreturn__));         // prints "abort" and stops with status 134, without running the atexit functions
int   atexit(void (*fn)(void)); // 0 on success, -1 when the 32 slots are taken
char* getenv(const char* name); // what `ceres run --env NAME=value` gave the program, or NULL (src/env.c)
int   setenv(const char* name, const char* value, int overwrite);   // 0, or -1 with errno EINVAL/ENOMEM
int   unsetenv(const char* name);
int   system(const char* cmd);  // there is no shell: 0 for system(NULL), else -1 with errno = ENOSYS

// ---- common non-standard helpers ----
char* itoa(int v, char* buf, int base);            // base 2..36; a negative value is signed only in base 10
char* utoa(unsigned int v, char* buf, int base);
char* ftoa(float v, char* buf, int decimals);      // like "%.*f", decimals 0..9; the buffer must hold the whole text
