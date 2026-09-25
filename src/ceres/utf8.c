// UTF-8. See ceres/utf8.h.
#include "ceres/utf8.h"
#include "uchar.h"

// One byte into a decoding in progress (the multibyte functions of uchar.h, wchar.h and stdlib.h use it too):
// 1 with the character in *out once it is complete, 0 while more bytes are needed, -1 when the byte cannot come
// next - and the state is then back between characters. The second byte is checked against the lead, which is
// what rules out the overlong forms, the surrogates and anything above U+10FFFF as soon as they can be seen.
int __utf8_feed(mbstate_t* st, unsigned char b, unsigned int* out)
{
    if (st->__need == 0)
    {
        if (b < 0x80u)
        {
            *out = b;
            return 1;
        }
        if (b >= 0xC2u && b <= 0xDFu)      { st->__bits = b & 0x1Fu; st->__need = 1; }
        else if (b >= 0xE0u && b <= 0xEFu) { st->__bits = b & 0x0Fu; st->__need = 2; }
        else if (b >= 0xF0u && b <= 0xF4u) { st->__bits = b & 0x07u; st->__need = 3; }
        else
            return -1;
        st->__lead = b;
        return 0;
    }
    unsigned int low = 0x80u, high = 0xBFu;
    if (st->__lead == 0xE0u) low = 0xA0u;          // below: overlong
    else if (st->__lead == 0xEDu) high = 0x9Fu;    // above: a surrogate
    else if (st->__lead == 0xF0u) low = 0x90u;     // below: overlong
    else if (st->__lead == 0xF4u) high = 0x8Fu;    // above: past U+10FFFF
    if (b < low || b > high)
    {
        st->__bits = 0;
        st->__need = 0;
        st->__lead = 0;
        return -1;
    }
    st->__lead = 0;                                  // only the second byte has limits of its own
    st->__bits = (st->__bits << 6) | (b & 0x3Fu);
    if (--st->__need > 0)
        return 0;
    *out = st->__bits;
    st->__bits = 0;
    return 1;
}

int utf8_decode(const char* s, size_t n, unsigned int* cp)
{
    if (n == 0)
        return 0;
    mbstate_t st = { 0 };
    for (size_t i = 0; i < n && i < UTF8_MAX; i++)
    {
        unsigned int c;
        int r = __utf8_feed(&st, (unsigned char)s[i], &c);
        if (r < 0)
            return -1;
        if (r > 0)
        {
            if (cp != 0)
                *cp = c;
            return (int)i + 1;
        }
    }
    return -1;                                       // cut short
}

int utf8_width(unsigned int cp)
{
    if (cp < 0x80u) return 1;
    if (cp < 0x800u) return 2;
    if (cp >= 0xD800u && cp <= 0xDFFFu) return 0;
    if (cp < 0x10000u) return 3;
    if (cp <= UNICODE_MAX) return 4;
    return 0;
}

int utf8_encode(unsigned int cp, char* out)
{
    int n = utf8_width(cp);
    switch (n)
    {
    case 1:
        out[0] = (char)cp;
        break;
    case 2:
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        break;
    case 3:
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        break;
    case 4:
        out[0] = (char)(0xF0u | (cp >> 18));
        out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (char)(0x80u | (cp & 0x3Fu));
        break;
    default:
        break;
    }
    return n;
}

unsigned int utf8_next(const char** s)
{
    const char* p = *s;
    if (*p == 0)
        return 0;
    unsigned int cp;
    int n = utf8_decode(p, UTF8_MAX, &cp);           // a terminator inside stops the decoding: it is no continuation
    if (n < 0)
    {
        *s = p + 1;
        return UTF8_REPLACEMENT;
    }
    *s = p + n;
    return cp;
}

size_t utf8_length(const char* s)
{
    size_t count = 0;
    while (utf8_next(&s) != 0)
        count++;
    return count;
}

int utf8_valid(const char* s, size_t n)
{
    size_t i = 0;
    while (i < n)
    {
        int k = utf8_decode(s + i, n - i, 0);
        if (k < 0)
            return 0;
        i += (size_t)k;
    }
    return 1;
}

const char* utf8_offset(const char* s, size_t index)
{
    while (index > 0 && utf8_next(&s) != 0)
        index--;
    return s;
}

size_t utf8_to_latin1(char* dst, size_t size, const char* src, char unknown)
{
    size_t length = 0;
    unsigned int cp;
    while ((cp = utf8_next(&src)) != 0)
    {
        if (length + 1 < size)
            dst[length] = cp <= 0xFFu ? (char)cp : unknown;
        length++;
    }
    if (size > 0)
        dst[length < size ? length : size - 1] = 0;
    return length;
}

size_t latin1_to_utf8(char* dst, size_t size, const char* src)
{
    size_t length = 0;
    int full = 0;                                    // a character did not fit: none after it is written either
    for (; *src != 0; src++)
    {
        char bytes[2];
        int n = utf8_encode((unsigned char)*src, bytes);
        if (!full && length + (size_t)n < size)
        {
            for (int i = 0; i < n; i++)
                dst[length + (size_t)i] = bytes[i];
        }
        else if (!full)
        {
            full = 1;
            if (size > 0)
                dst[length] = 0;
        }
        length += (size_t)n;
    }
    if (!full && size > 0)
        dst[length] = 0;
    return length;
}
