#pragma once

#include "stddef.h"

// General utilities. On this machine `long` and `long long` are `int` (32 bits), so labs/ldiv and the
// *ll* names below are aliases of the int versions, and `double` is `float`.

#define EXIT_SUCCESS  0
#define EXIT_FAILURE  1
#define RAND_MAX      32767
#define MB_CUR_MAX    1

struct __div_s { int quot; int rem; };
typedef struct __div_s div_t;
typedef struct __div_s ldiv_t;
typedef struct __div_s lldiv_t;

// ---- dynamic memory (src/malloc.c; the diagnostics are in ceres/heap.h) ----
void* malloc(size_t n);
void* calloc(size_t n, size_t size);
void* realloc(void* p, size_t n);
void  free(void* p);

// ---- text to number (src/strtox.c) ----
// Leading white space is skipped; `end`, when not NULL, receives the first character that was not
// part of the number (or `s` itself when there was no number). Overflow sets errno = ERANGE and
// returns the nearest limit. A base of 0 means "look at the prefix": 0x is 16, a bare 0 is 8.
int          atoi(const char* s);
int          atol(const char* s);
float        atof(const char* s);
int          strtol(const char* s, char** end, int base);
unsigned int strtoul(const char* s, char** end, int base);
float        strtof(const char* s, char** end);    // decimal, inf/infinity and nan; no hex floats
float        strtod(const char* s, char** end);    // == strtof
#define atoll     atol
#define strtoll   strtol
#define strtoull  strtoul
#define strtold   strtof

// ---- pseudo-random numbers: a 32-bit linear congruential generator ----
// The same seed always gives the same sequence. rand() returns 0..RAND_MAX; the low bits of an LCG are
// poor, so it hands out bits 30..16. Better generators are in ceres/rand.h.
int   rand(void);
void  srand(unsigned int seed);

// ---- arithmetic ----
int   abs(int v);              // abs(INT_MIN) is INT_MIN: the negation wraps
int   labs(int v);
int   llabs(int v);
div_t div(int num, int den);   // quotient truncated toward zero; a zero divisor sets the Trap flag
div_t ldiv(int num, int den);
div_t lldiv(int num, int den);

// ---- searching and sorting ----
// qsort is not stable. Elements of any size; the comparator receives pointers to two elements.
void  qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t n, size_t size,
              int (*cmp)(const void*, const void*));

// ---- the process ----
// The machine has no exit-status channel: `ceres run` reports success whenever the program halts
// cleanly, so the status argument is accepted and dropped.
void  exit(int status);        // runs the atexit functions (last registered first), then stops
void  _Exit(int status);       // stops at once
void  abort(void);             // prints "abort" and stops without running the atexit functions
int   atexit(void (*fn)(void)); // 0 on success, -1 when the 32 slots are taken
char* getenv(const char* name); // there is no environment: always NULL
int   system(const char* cmd);  // there is no shell: 0 for system(NULL), else -1 with errno = ENOSYS

// ---- common non-standard helpers ----
char* itoa(int v, char* buf, int base);            // base 2..36; a negative value is signed only in base 10
char* utoa(unsigned int v, char* buf, int base);
