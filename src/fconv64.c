// Exact binary64 <-> decimal conversion (fconv_priv.h): src/fconv.c's method for a double, which a program compiled
// with -fsoft-double has (ceres/f64.h). The value is handled as its bits, so this file needs no double itself.
//
// A double is m x 2^e exactly, with m below 2^53 and e from -1074 to 971: the integer m x 2^e when e >= 0 (up to
// 309 digits), and m x 5^-e divided by 10^-e when it is not (up to 767). Reading takes D x 10^E as num/den and 54
// bits of their quotient, 53 for the significand and one to round with, and whether anything is left over.
#include "fconv_priv.h"

#define LIMBS 132                    // 4224 bits: 780 digits over 10^1100 or so, shifted by the 54 bits of a quotient
#include "fconv_big.h"

typedef unsigned long long u64;

#define INF_BITS 0x7FF0000000000000ull

// ---- double to decimal ----

// The exact digits of |bits| (finite, nonzero) into out, without leading zeros; *x10 the power of ten of the
// first. Returns how many there are (at most 767).
static int exact_digits(u64 bits, char* out, int* x10)
{
    unsigned int biased = (unsigned int)((bits >> 52) & 0x7FFu);
    u64 m = bits & 0x000FFFFFFFFFFFFFull;
    int e;
    if (biased == 0)
        e = -1074;                                   // a subnormal: no hidden bit
    else
    {
        m |= 0x0010000000000000ull;
        e = (int)biased - 1075;
    }
    while ((m & 1u) == 0 && e < 0)                   // fewer digits to make, same value
    {
        m >>= 1;
        e++;
    }
    struct big I;
    I.w[0] = (unsigned int)m;
    I.w[1] = (unsigned int)(m >> 32);
    I.n = I.w[1] != 0 ? 2 : 1;
    int e10 = 0;
    if (e >= 0)
        big_shl(&I, e);
    else
    {
        big_mul_pow5(&I, -e);                        // m / 2^k = m 5^k / 10^k
        e10 = e;
    }
    unsigned int chunks[90];                         // nine digits each, least significant first
    int nc = 0;
    while (I.n > 0 && nc < 90)
        chunks[nc++] = big_divmod_small(&I, 1000000000u);
    int n = 0;
    for (int c = nc - 1; c >= 0; c--)
    {
        char nine[9];
        unsigned int x = chunks[c];
        for (int i = 8; i >= 0; i--)
        {
            nine[i] = (char)('0' + x % 10u);
            x /= 10u;
        }
        int start = 0;
        if (c == nc - 1)
            while (start < 8 && nine[start] == '0')
                start++;
        for (int i = start; i < 9; i++)
            out[n++] = nine[i];
    }
    *x10 = n - 1 + e10;
    return n;
}

// The first `keep` of the n digits in d, rounded to nearest (ties to even) on all of them, into out.
static void round_to(const char* d, int n, int keep, char* out, int* x10)
{
    for (int i = 0; i < keep; i++)
        out[i] = i < n ? d[i] : '0';
    if (keep >= n)
        return;
    int up = 0;
    if (d[keep] > '5')
        up = 1;
    else if (d[keep] == '5')
    {
        int rest = 0;
        for (int i = keep + 1; i < n; i++)
            if (d[i] != '0')
                rest = 1;
        up = rest || (keep > 0 && ((out[keep - 1] - '0') & 1));
    }
    if (!up)
        return;
    int i = keep - 1;
    while (i >= 0 && out[i] == '9')
    {
        out[i] = '0';
        i--;
    }
    if (i >= 0)
        out[i]++;
    else
    {
        out[0] = '1';
        (*x10)++;
    }
}

int __fconv64_digits(unsigned long long bits, int significant, int count, char* out, int* exp10)
{
    static char d[FCONV64_MAX_DIGITS];               // (static: 780 bytes the stack need not find)
    int x10;
    int n = exact_digits(bits & 0x7FFFFFFFFFFFFFFFull, d, &x10);
    int keep;
    if (significant)
        keep = count < 1 ? 1 : count;
    else
    {
        if (count > FCONV64_MAX_DIGITS)
            count = FCONV64_MAX_DIGITS;
        keep = x10 + 1 + count;
        if (keep <= 0)
        {
            int up = 0;
            if (keep == 0)
            {
                int rest = 0;
                for (int i = 1; i < n; i++)
                    if (d[i] != '0')
                        rest = 1;
                up = d[0] > '5' || (d[0] == '5' && rest);
            }
            if (!up)
                return 0;
            out[0] = '1';
            *exp10 = -count;
            return 1;
        }
    }
    if (keep > FCONV64_MAX_DIGITS)
        keep = FCONV64_MAX_DIGITS;
    int before = x10;
    round_to(d, n, keep, out, &x10);
    if (!significant && x10 != before && keep < FCONV64_MAX_DIGITS)
        out[keep++] = '0';
    *exp10 = x10;
    return keep;
}

// ---- decimal to double ----

// floor(num x 2^s / den), which the caller knows is below 2^56; *sticky when something is left over.
static u64 scaled_quotient(const struct big* num, const struct big* den, int s, int* sticky)
{
    static struct big a, b;                          // (static: they are big)
    big_copy(&a, num);
    big_copy(&b, den);
    if (s >= 0)
        big_shl(&a, s);
    else
        big_shl(&b, -s);
    big_shl(&b, 55);
    u64 q = 0;
    for (int bit = 55; bit >= 0; bit--)
    {
        if (big_cmp(&a, &b) >= 0)
        {
            big_sub(&a, &b);
            q |= 1ull << bit;
        }
        big_shr1(&b);
    }
    *sticky = a.n != 0;
    return q;
}

// The double nearest num/den (both nonzero), rounded to nearest with ties to even.
static u64 nearest(const struct big* num, const struct big* den, int extra_sticky, int* range)
{
    int s = 54 - (big_bits(num) - big_bits(den));   // num/den x 2^s is in [2^53, 2^55)
    int sticky;
    u64 q = scaled_quotient(num, den, s, &sticky);
    sticky |= extra_sticky;
    if (q >= (1ull << 54))
    {
        sticky |= (int)(q & 1u);
        q >>= 1;
        s--;
    }
    int biased = 53 - s + 1023;
    if (biased >= 2047)
    {
        *range = 1;
        return INF_BITS;
    }
    if (biased >= 1)
    {
        u64 m = q >> 1;
        if ((q & 1u) && (sticky || (m & 1u)))
            m++;
        if (m == (1ull << 53))
        {
            m >>= 1;
            biased++;
            if (biased >= 2047)
            {
                *range = 1;
                return INF_BITS;
            }
        }
        return ((u64)biased << 52) | (m & 0x000FFFFFFFFFFFFFull);
    }
    // Below the normal range: count in units of 2^-1074 (q in half units).
    q = scaled_quotient(num, den, 1075, &sticky);
    sticky |= extra_sticky;
    u64 m = q >> 1;
    int inexact = (q & 1u) || sticky;
    if ((q & 1u) && (sticky || (m & 1u)))
        m++;
    if (inexact && m < (1ull << 52))
        *range = 1;
    return m;                                        // 2^52 is the smallest normal, which the pattern spells
}

unsigned long long __fconv64_decimal(const char* digits, int n, int sticky, int exp10, int* range)
{
    while (n > 0 && digits[0] == '0')
    {
        digits++;
        n--;
    }
    if (n == 0)
        return 0;
    if (n + exp10 > 310)
    {
        *range = 1;                                  // at least 10^310: past the largest double
        return INF_BITS;
    }
    if (n + exp10 < -325)
    {
        *range = 1;                                  // below 10^-325: nearer 0 than the smallest subnormal
        return 0;
    }
    static struct big num, den;
    big_set(&num, 0);
    for (int i = 0; i < n && i < FCONV64_MAX_DIGITS; i++)
    {
        big_mul_small(&num, 10u);
        big_add_small(&num, (unsigned int)(digits[i] - '0'));
    }
    if (n > FCONV64_MAX_DIGITS)
    {
        for (int i = FCONV64_MAX_DIGITS; i < n; i++)
            if (digits[i] != '0')
                sticky = 1;
        exp10 += n - FCONV64_MAX_DIGITS;
    }
    if (sticky)
    {
        big_mul_small(&num, 10u);
        big_add_small(&num, 1u);
        exp10--;
    }
    big_set(&den, 1u);
    if (exp10 >= 0)
        big_mul_pow10(&num, exp10);
    else
        big_mul_pow10(&den, -exp10);
    return nearest(&num, &den, 0, range);
}

unsigned long long __fconv64_binary(unsigned long long h, int sticky, int exp2, int* range)
{
    if (h == 0)
        return 0;
    int top = 63;
    while (((h >> top) & 1u) == 0)
        top--;
    if (top + exp2 > 1024)
    {
        *range = 1;
        return INF_BITS;
    }
    if (top + exp2 < -1076)
    {
        *range = 1;
        return 0;
    }
    static struct big num, den;
    num.w[0] = (unsigned int)h;
    num.w[1] = (unsigned int)(h >> 32);
    num.n = num.w[1] != 0 ? 2 : 1;
    big_set(&den, 1u);
    if (exp2 >= 0)
        big_shl(&num, exp2);
    else
        big_shl(&den, -exp2);
    return nearest(&num, &den, sticky, range);
}
