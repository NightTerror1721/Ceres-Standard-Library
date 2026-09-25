#pragma once

#include "stddef.h"

// Wide characters. wchar_t is int and holds a Unicode code point (UTF-32), and the multibyte strings are UTF-8
// (see ceres/utf8.h and <uchar.h>), so a wide string is a string of code points. There are the conversions between
// the two and the string functions of wide strings; the wide stdio (fwprintf, getwc...) and wcsftime are not
// provided - printf's %lc and %ls write wide characters and strings as UTF-8, and scanf's read them.

#ifndef __CERES_MBSTATE_T
#define __CERES_MBSTATE_T
typedef struct
{
    unsigned int __bits;
    unsigned char __need;
    unsigned char __lead;
    unsigned char __pending;
    unsigned char __unused;
} mbstate_t;
#endif

typedef unsigned int wint_t;
#define WEOF ((wint_t)0xFFFFFFFFu)
#ifndef WCHAR_MIN
#define WCHAR_MIN (-2147483647 - 1)
#define WCHAR_MAX 2147483647
#endif

// ---- between multibyte (UTF-8) and wide ----
// mbrtowc and wcrtomb are <uchar.h>'s mbrtoc32 and c32rtomb for wchar_t; mbrlen is mbrtowc storing nothing. A
// wchar_t that is negative, a surrogate or above U+10FFFF is not a character: EILSEQ.
int    mbsinit(const mbstate_t* ps);                       // 1 when ps is between characters (or NULL)
size_t mbrlen(const char* s, size_t n, mbstate_t* ps);
size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps);
size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps);
// Whole strings: at most len wide characters (mbsrtowcs) or bytes (wcsrtombs) into dst, stopping before a
// character that would not fit whole. *src moves to where it stopped, or becomes NULL once the terminator is
// converted (which is not counted). With a NULL dst nothing is written, len is ignored and *src stays: the length
// the whole conversion takes. (size_t)-1 with EILSEQ when src is not UTF-8 (or holds a wide non-character).
size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps);
size_t wcsrtombs(char* dst, const wchar_t** src, size_t len, mbstate_t* ps);
wint_t btowc(int c);                                       // a byte that is a character alone (ASCII), else WEOF
int    wctob(wint_t c);                                    // ... and back, else EOF

// ---- wide strings, as their string.h namesakes ----
size_t   wcslen(const wchar_t* s);
int      wcscmp(const wchar_t* a, const wchar_t* b);
int      wcsncmp(const wchar_t* a, const wchar_t* b, size_t n);
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src);
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcscat(wchar_t* dst, const wchar_t* src);
wchar_t* wcschr(const wchar_t* s, wchar_t c);
wchar_t* wcsrchr(const wchar_t* s, wchar_t c);
wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle);
wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemmove(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemset(wchar_t* dst, wchar_t c, size_t n);
int      wmemcmp(const wchar_t* a, const wchar_t* b, size_t n);
wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n);
