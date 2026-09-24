#pragma once

#include "../string.h"

// The word-at-a-time strcpy, strcmp, strchr and memchr (asm/string_fast.casm) ARE the standard functions now:
// every program gets them through <string.h>. These names are the same routines at the same addresses, kept
// for code written when they were an opt-in, and CERES_STRING_FAST still maps the standard names onto them,
// which changes nothing any more.
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
