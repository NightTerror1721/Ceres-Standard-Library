# `<wchar.h>`

Wide characters. wchar_t is int and holds a Unicode code point (UTF-32), and the multibyte strings are UTF-8 (see ceres/utf8.h and <uchar.h>), so a wide string is a string of code points. There are the conversions between the two and the string functions of wide strings; the wide stdio (fwprintf, getwc...) and wcsftime are not provided - printf's %lc and %ls write wide characters and strings as UTF-8, and scanf's read them.

```c
#ifndef __CERES_WINT_T
#define __CERES_WINT_T
typedef unsigned int wint_t;
#endif
#ifndef WEOF
#define WEOF ((wint_t)0xFFFFFFFFu)
#endif
#ifndef WCHAR_MIN                              // <stdint.h> has them too; whichever comes first wins
#define WCHAR_MIN (-2147483647 - 1)
#define WCHAR_MAX 2147483647
#endif
```

## Between multibyte (UTF-8) and wide

```c
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
```

## Wide strings, as their string.h namesakes (wcscoll and wcsxfrm as in the one locale: wcscmp and a copy)

```c
size_t   wcslen(const wchar_t* s);
int      wcscmp(const wchar_t* a, const wchar_t* b);
int      wcsncmp(const wchar_t* a, const wchar_t* b, size_t n);
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src);
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcscat(wchar_t* dst, const wchar_t* src);
wchar_t* wcschr(const wchar_t* s, wchar_t c);
wchar_t* wcsrchr(const wchar_t* s, wchar_t c);
wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle);
wchar_t* wcsncat(wchar_t* dst, const wchar_t* src, size_t n);
size_t   wcsspn(const wchar_t* s, const wchar_t* accept);
size_t   wcscspn(const wchar_t* s, const wchar_t* reject);
wchar_t* wcspbrk(const wchar_t* s, const wchar_t* accept);
wchar_t* wcstok(wchar_t* s, const wchar_t* delim, wchar_t** save);
int      wcscoll(const wchar_t* a, const wchar_t* b);
size_t   wcsxfrm(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemmove(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wmemset(wchar_t* dst, wchar_t c, size_t n);
int      wmemcmp(const wchar_t* a, const wchar_t* b, size_t n);
wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n);
```
