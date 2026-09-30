// Multibyte (UTF-8) and wide characters: <uchar.h>, <wchar.h> and the multibyte functions of <stdlib.h>.
#include "uchar.h"
#include "wchar.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
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
    if (ps == NULL)
        ps = &own;
    if (s == NULL)
    {
        s = "";                                      // as the standard has it: mbrtoc32(NULL, "", 1, ps)
        n = 1;
        pc32 = NULL;
    }
    for (size_t used = 0; used < n; )
    {
        unsigned int c;
        int r = __utf8_feed(ps, (unsigned char)s[used++], &c);
        if (r < 0)
            return illegal(ps);
        if (r > 0)
        {
            if (pc32 != NULL)
                *pc32 = c;
            return c == 0 ? 0 : used;
        }
    }
    return (size_t)-2;                               // the character goes on past these n bytes
}

size_t c32rtomb(char* s, char32_t c32, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == NULL)
        ps = &own;
    if (s == NULL)
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
    if (ps == NULL)
        ps = &own;
    if (ps->__pending != 0)                           // the trail of a pair: read nothing
    {
        if (pc16 != NULL)
            *pc16 = (char16_t)ps->__bits;
        reset(ps);
        return (size_t)-3;
    }
    char32_t c;
    size_t r = mbrtoc32(&c, s, n, ps);
    if (r > 4 || s == NULL)
        return r;
    if (c >= 0x10000u)
    {
        c -= 0x10000u;
        if (pc16 != NULL)
            *pc16 = (char16_t)(0xD800u + (c >> 10));
        ps->__bits = 0xDC00u + (c & 0x3FFu);
        ps->__pending = 1;
    }
    else if (pc16 != NULL)
        *pc16 = (char16_t)c;
    return r;
}

size_t c16rtomb(char* s, char16_t c16, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == NULL)
        ps = &own;
    if (s == NULL)
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
    if (ps == NULL)
        ps = &own;
    if (ps->__pending != 0)                           // the next unit of the character read last
    {
        if (pc8 != NULL)
            *pc8 = (char8_t)(ps->__bits & 0xFFu);
        ps->__bits >>= 8;
        if (--ps->__pending == 0)
            reset(ps);
        return (size_t)-3;
    }
    char32_t c;
    size_t r = mbrtoc32(&c, s, n, ps);
    if (r > 4 || s == NULL)
        return r;
    char units[UTF8_MAX];
    int k = utf8_encode(c, units);
    if (pc8 != NULL)
        *pc8 = (char8_t)units[0];
    for (int i = k - 1; i >= 1; i--)
        ps->__bits = (ps->__bits << 8) | (unsigned char)units[i];
    ps->__pending = (unsigned char)(k - 1);
    return r;
}

size_t c8rtomb(char* s, char8_t c8, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == NULL)
        ps = &own;
    if (s == NULL)
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
    return ps == NULL || (ps->__need == 0 && ps->__pending == 0);
}

size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps)
{
    static mbstate_t own;
    char32_t c;
    size_t r = mbrtoc32(&c, s, n, ps != NULL ? ps : &own);
    if (r <= 4 && s != NULL && pwc != NULL)
        *pwc = (wchar_t)c;
    return r;
}

size_t mbrlen(const char* s, size_t n, mbstate_t* ps)
{
    static mbstate_t own;
    return mbrtowc(NULL, s, n, ps != NULL ? ps : &own);
}

size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == NULL)
        ps = &own;
    if (s != NULL && wc < 0)
        return illegal(ps);
    return c32rtomb(s, (char32_t)wc, ps);
}

size_t mbsrtowcs(wchar_t* dst, const char** src, size_t len, mbstate_t* ps)
{
    static mbstate_t own;
    if (ps == NULL)
        ps = &own;
    const char* s = *src;
    size_t count = 0;
    while (dst == NULL || count < len)
    {
        char32_t c;
        size_t r = mbrtoc32(&c, s, UTF8_MAX, ps);   // a terminator inside a character ends it as not UTF-8
        if (r == (size_t)-1 || r == (size_t)-2)
        {
            if (dst != NULL)
                *src = s;
            errno = EILSEQ;
            reset(ps);
            return (size_t)-1;
        }
        if (dst != NULL)
            dst[count] = (wchar_t)c;
        if (r == 0)
        {
            if (dst != NULL)
                *src = NULL;
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
    if (ps == NULL)
        ps = &own;
    const wchar_t* w = *src;
    size_t count = 0;
    for (;; w++)
    {
        char bytes[UTF8_MAX];
        int n = *w < 0 ? 0 : utf8_encode((unsigned int)*w, bytes);
        if (n == 0)
        {
            if (dst != NULL)
                *src = w;
            errno = EILSEQ;
            return (size_t)-1;
        }
        if (dst != NULL && count + (size_t)n > len)
            break;                                   // it would not fit whole
        if (dst != NULL)
            for (int i = 0; i < n; i++)
                dst[count + (size_t)i] = bytes[i];
        if (*w == 0)
        {
            if (dst != NULL)
                *src = NULL;
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
            return NULL;
    }
}

wchar_t* wcsrchr(const wchar_t* s, wchar_t c)
{
    const wchar_t* found = NULL;
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
    return NULL;
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
    while (s[n] != 0 && wcschr(accept, s[n]) != NULL)
        n++;
    return n;
}

size_t wcscspn(const wchar_t* s, const wchar_t* reject)
{
    size_t n = 0;
    while (s[n] != 0 && wcschr(reject, s[n]) == NULL)
        n++;
    return n;
}

wchar_t* wcspbrk(const wchar_t* s, const wchar_t* accept)
{
    s += wcscspn(s, accept);
    return *s != 0 ? (wchar_t*)s : NULL;
}

wchar_t* wcstok(wchar_t* s, const wchar_t* delim, wchar_t** save)
{
    if (s == NULL)
        s = *save;
    if (s == NULL)
        return NULL;
    s += wcsspn(s, delim);
    if (*s == 0)
    {
        *save = NULL;
        return NULL;
    }
    wchar_t* end = s + wcscspn(s, delim);
    if (*end != 0)
    {
        *end = 0;
        *save = end + 1;
    }
    else
        *save = NULL;
    return s;
}

int wcscoll(const wchar_t* a, const wchar_t* b)
{
    return wcscmp(a, b);                             // the "C" locale orders by code point
}

size_t wcsxfrm(wchar_t* dst, const wchar_t* src, size_t n)
{
    size_t length = wcslen(src);
    if (n != 0)
    {
        size_t copy = length < n - 1 ? length : n - 1;   // cut short, and terminated, as strxfrm does
        for (size_t i = 0; i < copy; i++)
            dst[i] = src[i];
        dst[copy] = 0;
    }
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
    return NULL;
}

// ---- <stdlib.h> ----
// UTF-8 has no shift states, so each of these says 0 to a NULL string ("not state-dependent").

static mbstate_t mb_own;

int mblen(const char* s, size_t n)
{
    return mbtowc(NULL, s, n);
}

int mbtowc(wchar_t* pwc, const char* s, size_t n)
{
    if (s == NULL)
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
    if (s == NULL)
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

// ---- text to number (wcstod, wcstol...): the strto* family (stdlib.h) over a wide string ----
//
// The narrow parsers read bytes, so the wide string is spelled as UTF-8 first (a number is ASCII, so this is just
// the digits), the narrow parser runs, and the end pointer is mapped back into the wide string.

static char* narrow_number(const wchar_t* s)
{
    char* n = (char*)malloc(wcslen(s) * UTF8_MAX + 1u);
    if (n == NULL)
        return NULL;
    size_t used = 0;
    for (; *s != 0; s++)
    {
        char unit[UTF8_MAX];
        int k = utf8_encode((unsigned int)*s, unit);
        if (k == 0)
            k = utf8_encode(UTF8_REPLACEMENT, unit);
        memcpy(n + used, unit, (size_t)k);
        used += (size_t)k;
    }
    n[used] = 0;
    return n;
}

// The index in `s` that the first `narrow` bytes of its UTF-8 reach.
static size_t wide_index(const wchar_t* s, size_t narrow)
{
    size_t seen = 0, i = 0;
    while (s[i] != 0 && seen < narrow)
    {
        char unit[UTF8_MAX];
        int k = utf8_encode((unsigned int)s[i], unit);
        if (k == 0)
            k = utf8_encode(UTF8_REPLACEMENT, unit);
        seen += (size_t)k;
        i++;
    }
    return i;
}

// One wrapper per narrow parser: spell the wide string as UTF-8, parse it, map the end back. WCSTO is for the
// integer ones (which take a base); WCSTO_REAL for the floating ones.
#define WCSTO(NAME, TYPE, PARSE, ZERO) \
    TYPE NAME(const wchar_t* s, wchar_t** end, int base) \
    { \
        char* n = narrow_number(s); \
        if (n == NULL) \
        { \
            if (end != NULL) *end = (wchar_t*)s; \
            errno = ENOMEM; \
            return ZERO; \
        } \
        char* at; \
        TYPE r = PARSE(n, &at, base); \
        if (end != NULL) *end = (wchar_t*)s + wide_index(s, (size_t)(at - n)); \
        int e = errno; \
        free(n); \
        errno = e; \
        return r; \
    }

#define WCSTO_REAL(NAME, TYPE, PARSE, ZERO) \
    TYPE NAME(const wchar_t* s, wchar_t** end) \
    { \
        char* n = narrow_number(s); \
        if (n == NULL) \
        { \
            if (end != NULL) *end = (wchar_t*)s; \
            errno = ENOMEM; \
            return ZERO; \
        } \
        char* at; \
        TYPE r = PARSE(n, &at); \
        if (end != NULL) *end = (wchar_t*)s + wide_index(s, (size_t)(at - n)); \
        int e = errno; \
        free(n); \
        errno = e; \
        return r; \
    }

WCSTO(wcstol, long, strtol, 0)
WCSTO(wcstoul, unsigned long, strtoul, 0)
WCSTO(wcstoll, long long, strtoll, 0)
WCSTO(wcstoull, unsigned long long, strtoull, 0)

intmax_t wcstoimax(const wchar_t* s, wchar_t** end, int base) { return wcstoll(s, end, base); }
uintmax_t wcstoumax(const wchar_t* s, wchar_t** end, int base) { return wcstoull(s, end, base); }

WCSTO_REAL(wcstod, double, strtod, 0.0)
WCSTO_REAL(wcstof, float, strtof, 0.0f)

long double wcstold(const wchar_t* s, wchar_t** end) { return (long double)wcstod(s, end); }

#undef WCSTO
#undef WCSTO_REAL
