// Text to number: atoi, atol, atoll, atof, strtol, strtoul, strtoll, strtoull, strtof, strtod.
#include "stdlib.h"
#include "ctype.h"
#include "errno.h"
#include "limits.h"
#include "stdint.h"
#include "math.h"

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

// 10^0 .. 10^10 are exact in a float (5^10 < 2^24), so one multiplication or division by one of them
// is a single correctly rounded operation.
static const float pow10_exact[11] = {
    1e0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f
};

static int matches_word(const char* p, const char* word)
{
    for (int i = 0; word[i] != 0; i++)
        if (tolower((unsigned char)p[i]) != word[i])
            return 0;
    return 1;
}

static float scale_by_pow10(float r, int e)
{
    while (e > 10 && !isinf(r))
    {
        r *= 1e10f;
        e -= 10;
    }
    while (e < -10 && r != 0.0f)
    {
        r /= 1e10f;
        e += 10;
    }
    if (r == 0.0f || isinf(r))
        return r;                       // the loops gave up early: e may still be out of the table's range
    if (e > 0)
        r *= pow10_exact[e];
    else if (e < 0)
        r /= pow10_exact[-e];
    return r;
}

// The result is correctly rounded when the digits fit 24 bits (up to 7 of them, or 16777216) and the
// decimal exponent is within +-10: then it is one exactly-rounded operation on exact operands. Longer
// inputs keep their first 9 digits and can be a rounding or two off; that is the price of having no
// wider type to compute in.
float strtof(const char* s, char** end)
{
    const char* p = s;
    if (end != 0)
        *end = (char*)s;
    while (isspace((unsigned char)*p))
        p++;
    int negative = 0;
    if (*p == '-')      { negative = 1; p++; }
    else if (*p == '+') { p++; }

    if (matches_word(p, "inf"))
    {
        if (end != 0)
            *end = (char*)(p + (matches_word(p, "infinity") ? 8 : 3));
        return negative ? -INFINITY : INFINITY;
    }
    if (matches_word(p, "nan"))
    {
        if (end != 0)
            *end = (char*)(p + 3);
        return NAN;
    }

    unsigned int mantissa = 0;      // the first 9 significant digits
    int dexp = 0;                   // the power of ten the mantissa still has to be scaled by
    int any = 0;
    while (*p >= '0' && *p <= '9')
    {
        any = 1;
        if (mantissa < 100000000u) mantissa = mantissa * 10u + (unsigned int)(*p - '0');
        else dexp++;                // a digit dropped from the integer part is a factor of ten
        p++;
    }
    if (*p == '.')
    {
        const char* q = p + 1;
        while (*q >= '0' && *q <= '9')
        {
            any = 1;
            if (mantissa < 100000000u) { mantissa = mantissa * 10u + (unsigned int)(*q - '0'); dexp--; }
            q++;
        }
        if (any)
            p = q;                  // "." alone is not a number; "5." is
    }
    if (!any)
        return 0.0f;

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
                if (e < 1000)
                    e = e * 10 + (*q - '0');
                q++;
            }
            dexp += exp_negative ? -e : e;
            p = q;
        }
    }
    if (end != 0)
        *end = (char*)p;

    float r = scale_by_pow10((float)mantissa, dexp);
    if (isinf(r))
        errno = ERANGE;                       // too big for a float
    else if (r == 0.0f && mantissa != 0u)
        errno = ERANGE;                       // so small it vanished
    return negative ? -r : r;
}

float strtod(const char* s, char** end) { return strtof(s, end); }
float atof(const char* s) { return strtof(s, 0); }
