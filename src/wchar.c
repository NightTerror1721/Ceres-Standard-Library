// Multibyte (UTF-8) and wide characters: <uchar.h>, <wchar.h> and the multibyte functions of <stdlib.h>.
#include "uchar.h"
#include "wchar.h"
#include "stdlib.h"
#include "stdio.h"
#include "errno.h"
#include "ceres/utf8.h"

int __utf8_feed(mbstate_t* st, unsigned char b, unsigned int* out);   // src/ceres/utf8.c

static void reset(mbstate_t* ps)
{
    ps->__bits = 0;
    ps->__need = 0;
    ps->__lead = 0;
    ps->__pending = 0;
}

static size_t illegal(mbstate_t* ps)
{
    reset(ps);
    errno = EILSEQ;
    return (size_t)-1;
}

// ---- <uchar.h> ----

size_t mbrtoc32(char32_t* pc32, const char* s, size_t n, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    if (s == 0)
    {
        s = "";                                      // as the standard has it: mbrtoc32(NULL, "", 1, ps)
        n = 1;
        pc32 = 0;
    }
    for (size_t used = 0; used < n; )
    {
        unsigned int c;
        int r = __utf8_feed(ps, (unsigned char)s[used++], &c);
        if (r < 0)
            return illegal(ps);
        if (r > 0)
        {
            if (pc32 != 0)
                *pc32 = c;
            return c == 0 ? 0 : used;
        }
    }
    return (size_t)-2;                               // the character goes on past these n bytes
}

size_t c32rtomb(char* s, char32_t c32, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    if (s == 0)
    {
        reset(ps);
        return 1;
    }
    int n = utf8_encode(c32, s);
    if (n == 0)
        return illegal(ps);
    reset(ps);
    return (size_t)n;
}

size_t mbrtoc16(char16_t* pc16, const char* s, size_t n, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    if (ps->__pending != 0)                           // the trail of a pair: read nothing
    {
        if (pc16 != 0)
            *pc16 = (char16_t)ps->__bits;
        reset(ps);
        return (size_t)-3;
    }
    char32_t c;
    size_t r = mbrtoc32(&c, s, n, ps);
    if (r > 4 || s == 0)
        return r;
    if (c >= 0x10000u)
    {
        c -= 0x10000u;
        if (pc16 != 0)
            *pc16 = (char16_t)(0xD800u + (c >> 10));
        ps->__bits = 0xDC00u + (c & 0x3FFu);
        ps->__pending = 1;
    }
    else if (pc16 != 0)
        *pc16 = (char16_t)c;
    return r;
}

size_t c16rtomb(char* s, char16_t c16, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    if (s == 0)
    {
        reset(ps);
        return 1;
    }
    unsigned int c = c16;
    if (ps->__pending != 0)                           // a lead is waiting for this trail
    {
        unsigned int lead = ps->__bits;
        if (c < 0xDC00u || c > 0xDFFFu)
            return illegal(ps);
        reset(ps);
        return (size_t)utf8_encode(0x10000u + ((lead - 0xD800u) << 10) + (c - 0xDC00u), s);
    }
    if (c >= 0xD800u && c <= 0xDBFFu)
    {
        ps->__bits = c;
        ps->__pending = 1;
        return 0;
    }
    if (c >= 0xDC00u && c <= 0xDFFFu)
        return illegal(ps);                          // a trail with no lead
    return (size_t)utf8_encode(c, s);
}

size_t mbrtoc8(char8_t* pc8, const char* s, size_t n, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    if (ps->__pending != 0)                           // the next unit of the character read last
    {
        if (pc8 != 0)
            *pc8 = (char8_t)(ps->__bits & 0xFFu);
        ps->__bits >>= 8;
        if (--ps->__pending == 0)
            reset(ps);
        return (size_t)-3;
    }
    char32_t c;
    size_t r = mbrtoc32(&c, s, n, ps);
    if (r > 4 || s == 0)
        return r;
    char units[UTF8_MAX];
    int k = utf8_encode(c, units);
    if (pc8 != 0)
        *pc8 = (char8_t)units[0];
    for (int i = k - 1; i >= 1; i--)
        ps->__bits = (ps->__bits << 8) | (unsigned char)units[i];
    ps->__pending = (unsigned char)(k - 1);
    return r;
}

size_t c8rtomb(char* s, char8_t c8, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    if (s == 0)
    {
        reset(ps);
        return 1;
    }
    unsigned int c;
    int r = __utf8_feed(ps, c8, &c);
    if (r < 0)
        return illegal(ps);
    if (r == 0)
        return 0;                                    // kept until the character is whole
    return (size_t)utf8_encode(c, s);
}

// ---- <wchar.h> ----

int mbsinit(const mbstate_t* ps)
{
    return ps == 0 || (ps->__need == 0 && ps->__pending == 0);
}

size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps)
{
    static mbstate_t own;
    char32_t c;
    size_t r = mbrtoc32(&c, s, n, ps != 0 ? ps : &own);
    if (r <= 4 && s != 0 && pwc != 0)
        *pwc = (wchar_t)c;
    return r;
}

size_t mbrlen(const char* s, size_t n, mbstate_t* ps)
{
    static mbstate_t own;
    return mbrtowc(0, s, n, ps != 0 ? ps : &own);
}

size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    if (s != 0 && wc < 0)
        return illegal(ps);
    return c32rtomb(s, (char32_t)wc, ps);
}

size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    const char* s = *src;
    size_t count = 0;
    while (dst == 0 || count < len)
    {
        char32_t c;
        size_t r = mbrtoc32(&c, s, UTF8_MAX, ps);   // a terminator inside a character ends it as not UTF-8
        if (r == (size_t)-1 || r == (size_t)-2)
        {
            if (dst != 0)
                *src = s;
            errno = EILSEQ;
            reset(ps);
            return (size_t)-1;
        }
        if (dst != 0)
            dst[count] = (wchar_t)c;
        if (r == 0)
        {
            if (dst != 0)
                *src = 0;
            return count;
        }
        s += r;
        count++;
    }
    *src = s;
    return count;
}

size_t wcsrtombs(char* dst, const wchar_t** src, size_t len, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == 0)
        ps = &own;
    const wchar_t* w = *src;
    size_t count = 0;
    for (;; w++)
    {
        char bytes[UTF8_MAX];
        int n = *w < 0 ? 0 : utf8_encode((unsigned int)*w, bytes);
        if (n == 0)
        {
            if (dst != 0)
                *src = w;
            errno = EILSEQ;
            return (size_t)-1;
        }
        if (dst != 0 && count + (size_t)n > len)
            break;                                   // it would not fit whole
        if (dst != 0)
            for (int i = 0; i < n; i++)
                dst[count + (size_t)i] = bytes[i];
        if (*w == 0)
        {
            if (dst != 0)
                *src = 0;
            return count;
        }
        count += (size_t)n;
    }
    *src = w;
    return count;
}

wint_t btowc(int c)
{
    return c >= 0 && c < 0x80 ? (wint_t)c : WEOF;
}

int wctob(wint_t c)
{
    return c < 0x80u ? (int)c : EOF;
}

size_t wcslen(const wchar_t* s)
{
    size_t n = 0;
    while (s[n] != 0)
        n++;
    return n;
}

int wcscmp(const wchar_t* a, const wchar_t* b)
{
    while (*a != 0 && *a == *b)
    {
        a++;
        b++;
    }
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

int wcsncmp(const wchar_t* a, const wchar_t* b, size_t n)
{
    for (; n > 0; n--, a++, b++)
    {
        if (*a != *b)
            return *a < *b ? -1 : 1;
        if (*a == 0)
            break;
    }
    return 0;
}

wchar_t* wcscpy(wchar_t* dst, const wchar_t* src)
{
    wchar_t* d = dst;
    while ((*d++ = *src++) != 0)
        ;
    return dst;
}

wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i] != 0; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
    return dst;
}

wchar_t* wcscat(wchar_t* dst, const wchar_t* src)
{
    wcscpy(dst + wcslen(dst), src);
    return dst;
}

wchar_t* wcschr(const wchar_t* s, wchar_t c)
{
    for (;; s++)
    {
        if (*s == c)
            return (wchar_t*)s;
        if (*s == 0)
            return 0;
    }
}

wchar_t* wcsrchr(const wchar_t* s, wchar_t c)
{
    const wchar_t* found = 0;
    for (;; s++)
    {
        if (*s == c)
            found = s;
        if (*s == 0)
            return (wchar_t*)found;
    }
}

wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle)
{
    size_t n = wcslen(needle);
    for (; *haystack != 0 || n == 0; haystack++)
    {
        if (wcsncmp(haystack, needle, n) == 0)
            return (wchar_t*)haystack;
        if (*haystack == 0)
            break;
    }
    return 0;
}

wchar_t* wcsncat(wchar_t* dst, const wchar_t* src, size_t n)
{
    wchar_t* d = dst + wcslen(dst);
    size_t i = 0;
    for (; i < n && src[i] != 0; i++)
        d[i] = src[i];
    d[i] = 0;
    return dst;
}

size_t wcsspn(const wchar_t* s, const wchar_t* accept)
{
    size_t n = 0;
    while (s[n] != 0 && wcschr(accept, s[n]) != 0)
        n++;
    return n;
}

size_t wcscspn(const wchar_t* s, const wchar_t* reject)
{
    size_t n = 0;
    while (s[n] != 0 && wcschr(reject, s[n]) == 0)
        n++;
    return n;
}

wchar_t* wcspbrk(const wchar_t* s, const wchar_t* accept)
{
    s += wcscspn(s, accept);
    return *s != 0 ? (wchar_t*)s : 0;
}

wchar_t* wcstok(wchar_t* s, const wchar_t* delim, wchar_t** save)
{
    if (s == 0)
        s = *save;
    if (s == 0)
        return 0;
    s += wcsspn(s, delim);
    if (*s == 0)
    {
        *save = 0;
        return 0;
    }
    wchar_t* end = s + wcscspn(s, delim);
    if (*end != 0)
    {
        *end = 0;
        *save = end + 1;
    }
    else
        *save = 0;
    return s;
}

int wcscoll(const wchar_t* a, const wchar_t* b)
{
    return wcscmp(a, b);                             // the "C" locale orders by code point
}

size_t wcsxfrm(wchar_t* dst, const wchar_t* src, size_t n)
{
    size_t length = wcslen(src);
    if (length < n)
        wcscpy(dst, src);
    return length;
}

wchar_t* wmemcpy(wchar_t* dst, const wchar_t* src, size_t n)
{
    for (size_t i = 0; i < n; i++)
        dst[i] = src[i];
    return dst;
}

wchar_t* wmemmove(wchar_t* dst, const wchar_t* src, size_t n)
{
    if (dst < src)
        for (size_t i = 0; i < n; i++)
            dst[i] = src[i];
    else
        for (size_t i = n; i > 0; i--)
            dst[i - 1] = src[i - 1];
    return dst;
}

wchar_t* wmemset(wchar_t* dst, wchar_t c, size_t n)
{
    for (size_t i = 0; i < n; i++)
        dst[i] = c;
    return dst;
}

int wmemcmp(const wchar_t* a, const wchar_t* b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    return 0;
}

wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (s[i] == c)
            return (wchar_t*)(s + i);
    return 0;
}

// ---- <stdlib.h> ----
// UTF-8 has no shift states, so each of these says 0 to a NULL string ("not state-dependent").

static mbstate_t mb_own;

int mblen(const char* s, size_t n)
{
    return mbtowc(0, s, n);
}

int mbtowc(wchar_t* pwc, const char* s, size_t n)
{
    if (s == 0)
    {
        reset(&mb_own);
        return 0;
    }
    size_t r = mbrtowc(pwc, s, n, &mb_own);
    if (r == (size_t)-2)
    {
        reset(&mb_own);                              // incomplete: not a character, as far as these n bytes go
        errno = EILSEQ;
        return -1;
    }
    return r == (size_t)-1 ? -1 : (int)r;
}

int wctomb(char* s, wchar_t wc)
{
    if (s == 0)
        return 0;
    mbstate_t st = { 0 };
    size_t r = wcrtomb(s, wc, &st);
    return r == (size_t)-1 ? -1 : (int)r;
}

size_t mbstowcs(wchar_t* dst, const char* src, size_t n)
{
    mbstate_t st = { 0 };
    return mbsrtowcs(dst, &src, n, &st);
}

size_t wcstombs(char* dst, const wchar_t* src, size_t n)
{
    mbstate_t st = { 0 };
    return wcsrtombs(dst, &src, n, &st);
}
