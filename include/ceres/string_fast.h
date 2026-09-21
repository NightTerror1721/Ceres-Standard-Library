#pragma once

#include "../string.h"

// Faster strcpy, strcmp, strchr and memchr, in assembly (asm/string_fast.casm). They answer exactly what the
// standard functions do, in about a third of the instructions at -O2 and a thirtieth at -O0 (4096 bytes: strcpy takes
// 11305 instructions against 32797, strcmp 13356 against 40995).
//
// Use them by name, or include this header AFTER <string.h> with CERES_STRING_FAST defined and the standard
// names in the rest of the file mean these:
//
//   #define CERES_STRING_FAST
//   #include "ceres/string_fast.h"
//   ... strcmp(a, b) now calls strcmp_fast ...
//
// The library itself is not affected: only code compiled after the header sees the mapping.
char* strcpy_fast(char* restrict dst, const char* restrict src);
int   strcmp_fast(const char* a, const char* b);
char* strchr_fast(const char* s, int c);
void* memchr_fast(const void* p, int c, size_t n);

#ifdef CERES_STRING_FAST
#define strcpy  strcpy_fast
#define strcmp  strcmp_fast
#define strchr  strchr_fast
#define memchr  memchr_fast
#endif
