// Text to number: atoi, atol, atoll, atof, strtol, strtoul, strtoll, strtoull, strtof, strtod.
#include "fconv_priv.h"
#include "stdlib.h"
#include "ctype.h"
#include "errno.h"
#include "limits.h"
#include "stdint.h"
#include "math.h"
#include "string.h"

// ---- integers ----

// Reads the sign, the base prefix and the digits. Returns the magnitude; *overflow is set when it
// did not fit 32 bits (the returned value is then meaningless). *end gets the first unread character,
// or `s` itself when there were no digits.
static unsigned int parse_magnitude(const char* s, char** end, int base, int* negative, int* overflow)
{
    const char* p = s;
    *negative = 0;
    *overflow = 0;
    if (end != 0)
        *end = (char*)s;
    if (base < 0 || base == 1 || base > 36)
    {
        errno = EINVAL;
        return 0;
    }

    while (isspace((unsigned char)*p))
        p++;
    if (*p == '-')      { *negative = 1; p++; }
    else if (*p == '+') { p++; }

    // "0x" is a prefix only when a hex digit follows; otherwise the number is the "0" alone.
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2]))
    {
        p += 2;
        base = 16;
    }
    else if (base == 0)
    {
        base = (p[0] == '0') ? 8 : 10;
    }

    unsigned int value = 0;
    int any = 0;
    for (;; p++)
    {
        int c = (unsigned char)*p;
        int digit;
        if (c >= '0' && c <= '9')      digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;
        if (digit >= base)
            break;
        any = 1;
        if (value > (UINT_MAX - (unsigned int)digit) / (unsigned int)base)
            *overflow = 1;
        else
            value = value * (unsigned int)base + (unsigned int)digit;
    }
    if (any && end != 0)
        *end = (char*)p;
    return value;
}

unsigned int strtoul(const char* s, char** end, int base)
{
    int negative, overflow;
    unsigned int v = parse_magnitude(s, end, base, &negative, &overflow);
    if (overflow)
    {
        errno = ERANGE;
        return UINT_MAX;
    }
    return negative ? 0u - v : v;       // "-1" is UINT_MAX, as in C
}

int strtol(const char* s, char** end, int base)
{
    int negative, overflow;
    unsigned int v = parse_magnitude(s, end, base, &negative, &overflow);
    if (negative)
    {
        if (overflow || v > 2147483648u)
        {
            errno = ERANGE;
            return INT_MIN;
        }
        return (int)(0u - v);
    }
    if (overflow || v > 2147483647u)
    {
        errno = ERANGE;
        return INT_MAX;
    }
    return (int)v;
}

int atoi(const char* s) { return strtol(s, 0, 10); }
int atol(const char* s) { return strtol(s, 0, 10); }

// ---- 64-bit integers ----

// The same parse as parse_magnitude(), but accumulating into a 64-bit magnitude, so a value the 32-bit
// one would call overflow is the answer here. *overflow is set only past 2^64-1.
static uint64_t parse_magnitude64(const char* s, char** end, int base, int* negative, int* overflow)
{
    const char* p = s;
    *negative = 0;
    *overflow = 0;
    if (end != 0)
        *end = (char*)s;
    if (base < 0 || base == 1 || base > 36)
    {
        errno = EINVAL;
        return 0;
    }

    while (isspace((unsigned char)*p))
        p++;
    if (*p == '-')      { *negative = 1; p++; }
    else if (*p == '+') { p++; }

    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2]))
    {
        p += 2;
        base = 16;
    }
    else if (base == 0)
    {
        base = (p[0] == '0') ? 8 : 10;
    }

    uint64_t value = 0;
    int any = 0;
    for (;; p++)
    {
        int c = (unsigned char)*p;
        int digit;
        if (c >= '0' && c <= '9')      digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;
        if (digit >= base)
            break;
        any = 1;
        // The magnitude is unsigned 64-bit; the boundary is 2^64-1, spelled as the max value.
        if (value > (0xFFFFFFFFFFFFFFFFULL - (uint64_t)digit) / (uint64_t)base)
            *overflow = 1;
        else
            value = value * (uint64_t)base + (uint64_t)digit;
    }
    if (any && end != 0)
        *end = (char*)p;
    return value;
}

unsigned long long strtoull(const char* s, char** end, int base)
{
    int negative, overflow;
    uint64_t v = parse_magnitude64(s, end, base, &negative, &overflow);
    if (overflow)
    {
        errno = ERANGE;
        return 0xFFFFFFFFFFFFFFFFULL;
    }
    return negative ? (uint64_t)(0ULL - v) : v;   // "-1" is ULLONG_MAX, as in C
}

long long strtoll(const char* s, char** end, int base)
{
    int negative, overflow;
    uint64_t v = parse_magnitude64(s, end, base, &negative, &overflow);
    if (negative)
    {
        if (overflow || v > 0x8000000000000000ULL)
        {
            errno = ERANGE;
            return (-9223372036854775807LL - 1);   // LLONG_MIN
        }
        return (long long)(0ULL - v);
    }
    if (overflow || v > 0x7FFFFFFFFFFFFFFFULL)
    {
        errno = ERANGE;
        return 9223372036854775807LL;              // LLONG_MAX
    }
    return (long long)v;
}

long long atoll(const char* s) { return strtoll(s, 0, 10); }

// ---- floating point ----

static int matches_word(const char* p, const char* word)
{
    for (int i = 0; word[i] != 0; i++)
        if (tolower((unsigned char)p[i]) != word[i])
            return 0;
    return 1;
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// A floating number as the text has it, before it is rounded to a format: inf, nan, a hexadecimal mantissa times a
// power of two, or decimal digits times a power of ten. strtof rounds it to a float and strtod (under -fsoft-double)
// to a double, so the two read exactly the same text.
enum { REAL_NONE, REAL_INF, REAL_NAN, REAL_HEX, REAL_DECIMAL };

struct real_text
{
    int kind;
    int negative;
    unsigned long long h;                            // REAL_HEX: the mantissa's first 60 bits
    int nd;                                          // REAL_DECIMAL: the significant digits kept
    int sticky;                                      // a nonzero digit past what was kept
    int exp;                                         // the power of two (hex) or of ten (decimal) still to apply
};

// "0x1.8p3": hexadecimal digits with an optional point, then an optional binary exponent. p points after "0x";
// returns the end, or 0 when no digit followed (the caller then takes the "0" alone).
static const char* read_hex_float(const char* p, struct real_text* t)
{
    unsigned long long h = 0;
    int exp2 = 0;
    int any = 0;
    int sticky = 0;
    int point = 0;
    for (;; p++)
    {
        if (*p == '.' && !point)
        {
            point = 1;
            continue;
        }
        int d = hex_value((unsigned char)*p);
        if (d < 0)
            break;
        any = 1;
        if ((h >> 60) == 0)
        {
            h = (h << 4) | (unsigned int)d;
            if (point) exp2 -= 4;
        }
        else
        {
            if (d != 0) sticky = 1;                  // past 60 bits: only "not zero" matters
            if (!point) exp2 += 4;
        }
    }
    if (!any)
        return 0;
    if (*p == 'p' || *p == 'P')
    {
        const char* q = p + 1;
        int negative = 0;
        if (*q == '-')      { negative = 1; q++; }
        else if (*q == '+') { q++; }
        if (*q >= '0' && *q <= '9')
        {
            int e = 0;
            while (*q >= '0' && *q <= '9')
            {
                if (e < 100000) e = e * 10 + (*q - '0');
                q++;
            }
            exp2 += negative ? -e : e;
            p = q;
        }
    }
    t->kind = REAL_HEX;
    t->h = h;
    t->sticky = sticky;
    t->exp = exp2;
    return p;
}

// Reads a floating number from s into t, keeping at most `max_digits` significant decimal digits in `digits`: the
// end of what was read, or s itself when there was no number (t->kind is then REAL_NONE).
static const char* scan_real(const char* s, struct real_text* t, char* digits, int max_digits)
{
    const char* p = s;
    t->kind = REAL_NONE;
    t->negative = 0;
    t->nd = 0;
    t->sticky = 0;
    t->exp = 0;
    while (isspace((unsigned char)*p))
        p++;
    if (*p == '-')      { t->negative = 1; p++; }
    else if (*p == '+') { p++; }

    if (matches_word(p, "inf"))
    {
        t->kind = REAL_INF;
        return p + (matches_word(p, "infinity") ? 8 : 3);
    }
    if (matches_word(p, "nan"))
    {
        t->kind = REAL_NAN;
        return p + 3;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        const char* after = read_hex_float(p + 2, t);
        if (after != 0)
            return after;
        t->kind = REAL_DECIMAL;                      // "0x" alone: the 0
        return p + 1;
    }
    int nd = 0;                                      // significant digits kept
    int sticky = 0;                                  // a nonzero digit past them
    int dexp = 0;                                    // the power of ten the digits still need
    int any = 0;
    int point = 0;
    for (;; p++)
    {
        if (*p == '.' && !point)
        {
            point = 1;
            continue;
        }
        if (*p < '0' || *p > '9')
            break;
        any = 1;
        if (nd == 0 && *p == '0')
        {
            if (point) dexp--;                       // a leading zero after the point moves the digits down
            continue;
        }
        if (nd < max_digits)
        {
            digits[nd++] = *p;
            if (point) dexp--;
        }
        else
        {
            if (*p != '0') sticky = 1;
            if (!point) dexp++;
        }
    }
    if (!any)
        return s;                                    // "." alone is not a number; "5." is
    if (*p == 'e' || *p == 'E')
    {
        const char* q = p + 1;
        int exp_negative = 0;
        if (*q == '-')      { exp_negative = 1; q++; }
        else if (*q == '+') { q++; }
        if (*q >= '0' && *q <= '9')
        {
            int e = 0;
            while (*q >= '0' && *q <= '9')
            {
                if (e < 100000) e = e * 10 + (*q - '0');
                q++;
            }
            dexp += exp_negative ? -e : e;
            p = q;
        }
    }
    t->kind = REAL_DECIMAL;
    t->nd = nd;
    t->sticky = sticky;
    t->exp = dexp;
    return p;
}

// Correctly rounded: the result is the float nearest the decimal value written, ties to even (src/fconv.c works
// on its exact digits), whatever their number. "0x" introduces a hexadecimal float ("0x1.8p3" is 12), and
// "inf", "infinity" and "nan" are read in any case. ERANGE when the value overflows to infinity or underflows
// (to zero, or to a subnormal that is not exact).
float strtof(const char* s, char** end)
{
    char digits[FCONV_MAX_DIGITS];
    struct real_text t;
    const char* after = scan_real(s, &t, digits, FCONV_MAX_DIGITS);
    if (end != 0)
        *end = (char*)after;
    int range = 0;
    float r = 0.0f;
    switch (t.kind)
    {
    case REAL_NONE:    return 0.0f;
    case REAL_INF:     r = INFINITY; break;
    case REAL_NAN:     return NAN;
    case REAL_HEX:     r = __fconv_binary(t.h, t.sticky, t.exp, &range); break;
    default:           r = __fconv_decimal(digits, t.nd, t.sticky, t.exp, &range); break;
    }
    if (range)
        errno = ERANGE;
    return t.negative ? -r : r;
}

#ifdef __CERES_SOFT_DOUBLE__
// The same for a real double (-fsoft-double), rounded once to binary64 from the exact digits.
double strtod(const char* s, char** end)
{
    static char digits[FCONV64_MAX_DIGITS];          // (static: 780 bytes)
    struct real_text t;
    const char* after = scan_real(s, &t, digits, FCONV64_MAX_DIGITS);
    if (end != 0)
        *end = (char*)after;
    int range = 0;
    unsigned long long bits = 0;
    switch (t.kind)
    {
    case REAL_NONE:    break;
    case REAL_INF:     bits = 0x7FF0000000000000ull; break;
    case REAL_NAN:     bits = 0x7FF8000000000000ull; break;
    case REAL_HEX:     bits = __fconv64_binary(t.h, t.sticky, t.exp, &range); break;
    default:           bits = __fconv64_decimal(digits, t.nd, t.sticky, t.exp, &range); break;
    }
    if (range)
        errno = ERANGE;
    if (t.negative && t.kind != REAL_NAN && t.kind != REAL_NONE)
        bits |= 0x8000000000000000ull;
    double d;
    memcpy(&d, &bits, 8);
    return d;
}

double atof(const char* s) { return strtod(s, 0); }
#else
float strtod(const char* s, char** end) { return strtof(s, end); }
float atof(const char* s) { return strtof(s, 0); }
#endif
