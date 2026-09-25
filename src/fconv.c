// Exact float <-> decimal conversion (fconv_priv.h), on big integers.
//
// A float is m x 2^e exactly, with m below 2^24 and e from -149 to 104. Its decimal expansion is exact too: the
// integer m x 2^e when e >= 0, and m x 5^-e divided by 10^-e when it is not - at most 112 digits. Printing takes
// those digits and rounds them where it is asked to, to nearest with ties to even, on the exact value: no digit
// is ever noise.
//
// Reading goes the other way: D x 10^E is a fraction num/den of big integers, and the float nearest it comes from
// 25 bits of their quotient (24 for the mantissa and one to round with) and whether anything is left over. The
// digits past the 120th only matter as "not all zero", which one more digit 1 stands for exactly.
#include "fconv_priv.h"

#define LIMBS 40                     // 1280 bits: more than the largest numerator or shifted denominator here

struct big
{
    unsigned int w[LIMBS];
    int n;                           // limbs in use; 0 is zero
};

static void big_set(struct big* b, unsigned int x)
{
    b->w[0] = x;
    b->n = x != 0 ? 1 : 0;
}

static void big_copy(struct big* to, const struct big* from)
{
    to->n = from->n;
    for (int i = 0; i < from->n; i++)
        to->w[i] = from->w[i];
}

static void big_mul_small(struct big* b, unsigned int m)
{
    unsigned long long carry = 0;
    for (int i = 0; i < b->n; i++)
    {
        unsigned long long t = (unsigned long long)b->w[i] * m + carry;
        b->w[i] = (unsigned int)t;
        carry = t >> 32;
    }
    if (carry != 0 && b->n < LIMBS)
        b->w[b->n++] = (unsigned int)carry;
}

static void big_add_small(struct big* b, unsigned int a)
{
    for (int i = 0; a != 0; i++)
    {
        if (i == b->n)
        {
            if (b->n == LIMBS)
                return;
            b->w[b->n++] = 0;
        }
        unsigned int before = b->w[i];
        b->w[i] = before + a;
        a = b->w[i] < before ? 1u : 0u;
    }
}

static void big_shl(struct big* b, int k)
{
    if (b->n == 0 || k <= 0)
        return;
    int limbs = k / 32, bits = k % 32;
    int n = b->n + limbs + 1;
    if (n > LIMBS)
        n = LIMBS;
    for (int i = n - 1; i >= 0; i--)
    {
        int from = i - limbs;
        unsigned int hi = from >= 0 && from < b->n ? b->w[from] : 0u;
        unsigned int lo = from - 1 >= 0 && from - 1 < b->n ? b->w[from - 1] : 0u;
        b->w[i] = bits == 0 ? hi : (hi << bits) | (lo >> (32 - bits));
    }
    b->n = n;
    while (b->n > 0 && b->w[b->n - 1] == 0)
        b->n--;
}

static void big_shr1(struct big* b)
{
    for (int i = 0; i < b->n; i++)
    {
        unsigned int next = i + 1 < b->n ? b->w[i + 1] : 0u;
        b->w[i] = (b->w[i] >> 1) | (next << 31);
    }
    while (b->n > 0 && b->w[b->n - 1] == 0)
        b->n--;
}

static int big_cmp(const struct big* a, const struct big* b)
{
    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->w[i] != b->w[i])
            return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}

// a -= b, where a >= b.
static void big_sub(struct big* a, const struct big* b)
{
    unsigned int borrow = 0;
    for (int i = 0; i < a->n; i++)
    {
        unsigned int bw = i < b->n ? b->w[i] : 0u;
        unsigned int t = a->w[i] - bw - borrow;
        borrow = (a->w[i] < bw || (a->w[i] == bw && borrow)) ? 1u : 0u;
        a->w[i] = t;
    }
    while (a->n > 0 && a->w[a->n - 1] == 0)
        a->n--;
}

static int big_bits(const struct big* b)
{
    if (b->n == 0)
        return 0;
    return (b->n - 1) * 32 + 32 - __builtin_clz(b->w[b->n - 1]);
}

// b /= d, and the remainder.
static unsigned int big_divmod_small(struct big* b, unsigned int d)
{
    unsigned long long rem = 0;
    for (int i = b->n - 1; i >= 0; i--)
    {
        unsigned long long t = (rem << 32) | b->w[i];
        b->w[i] = (unsigned int)(t / d);
        rem = t % d;
    }
    while (b->n > 0 && b->w[b->n - 1] == 0)
        b->n--;
    return (unsigned int)rem;
}

static void big_mul_pow(struct big* b, unsigned int base_pow_chunk, int chunk, unsigned int base, int k)
{
    while (k >= chunk)
    {
        big_mul_small(b, base_pow_chunk);
        k -= chunk;
    }
    unsigned int rest = 1;
    for (int i = 0; i < k; i++)
        rest *= base;
    if (rest != 1)
        big_mul_small(b, rest);
}

static void big_mul_pow10(struct big* b, int k) { big_mul_pow(b, 1000000000u, 9, 10u, k); }
static void big_mul_pow5(struct big* b, int k)  { big_mul_pow(b, 1220703125u, 13, 5u, k); }

// ---- float to decimal ----

union fbits { float f; unsigned int u; };

// The exact digits of |v| (finite, nonzero) into out[128], without leading zeros; *x10 the power of ten of the
// first. Returns how many there are.
static int exact_digits(float v, char* out, int* x10)
{
    union fbits b;
    b.f = v;
    unsigned int biased = (b.u >> 23) & 255u;
    unsigned int m = b.u & 0x7FFFFFu;
    int e;
    if (biased == 0)
        e = -149;                                    // a subnormal: no hidden bit
    else
    {
        m |= 0x800000u;
        e = (int)biased - 150;
    }
    while ((m & 1u) == 0 && e < 0)                   // fewer digits to make, same value
    {
        m >>= 1;
        e++;
    }
    struct big I;
    big_set(&I, m);
    int e10 = 0;
    if (e >= 0)
        big_shl(&I, e);
    else
    {
        big_mul_pow5(&I, -e);                        // m / 2^k = m 5^k / 10^k
        e10 = e;
    }
    // Nine digits at a time, least significant first.
    unsigned int chunks[16];
    int nc = 0;
    while (I.n > 0 && nc < 16)
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
                start++;                             // no leading zeros
        for (int i = start; i < 9; i++)
            out[n++] = nine[i];
    }
    *x10 = n - 1 + e10;
    return n;
}

// The first `keep` of the n digits in d, rounded to nearest (ties to even) on all of them, into out; *x10 moves up
// when the rounding carries past the first. Digits past n are zeros.
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
        out[0] = '1';                                // 999 -> 1000: one more power of ten
        (*x10)++;
    }
}

int __fconv_digits(float v, int significant, int count, char* out, int* exp10)
{
    char d[128];
    int x10;
    int n = exact_digits(v, d, &x10);
    int keep;
    if (significant)
        keep = count < 1 ? 1 : count;
    else
    {
        keep = x10 + 1 + count;                      // the digits down to 10^-count
        if (keep <= 0)
        {
            // Below the last place asked for: it rounds to 0, or up to one unit of it.
            int up = 0;
            if (keep == 0)
            {
                int rest = 0;
                for (int i = 1; i < n; i++)
                    if (d[i] != '0')
                        rest = 1;
                up = d[0] > '5' || (d[0] == '5' && rest);   // exactly half rounds to the even 0
            }
            if (!up)
                return 0;
            out[0] = '1';
            *exp10 = -count;
            return 1;
        }
    }
    if (keep > FCONV_MAX_DIGITS)
        keep = FCONV_MAX_DIGITS;                     // the rest are zeros, which the caller puts out
    int before = x10;
    round_to(d, n, keep, out, &x10);
    if (!significant && x10 != before && keep < FCONV_MAX_DIGITS)
        out[keep++] = '0';                           // a carry to a new first digit: one more place to show
    *exp10 = x10;
    return keep;
}

int __fconv_shortest(float v, char* out, int* exp10)
{
    union fbits want;
    want.f = v < 0.0f ? -v : v;
    for (int n = 1; n < 9; n++)
    {
        int x10;
        __fconv_digits(want.f, 1, n, out, &x10);
        int range = 0;
        union fbits got;
        got.f = __fconv_decimal(out, n, 0, x10 - (n - 1), &range);
        if (got.u == want.u)
        {
            *exp10 = x10;
            return n;
        }
    }
    return __fconv_digits(want.f, 1, 9, out, exp10);   // nine digits always read back
}

// ---- decimal to float ----

// floor(num x 2^s / den), which the caller knows is below 2^27; *sticky when something is left over.
static unsigned int scaled_quotient(const struct big* num, const struct big* den, int s, int* sticky)
{
    struct big a, b;
    big_copy(&a, num);
    big_copy(&b, den);
    if (s >= 0)
        big_shl(&a, s);
    else
        big_shl(&b, -s);
    big_shl(&b, 26);
    unsigned int q = 0;
    for (int bit = 26; bit >= 0; bit--)
    {
        if (big_cmp(&a, &b) >= 0)
        {
            big_sub(&a, &b);
            q |= 1u << bit;
        }
        big_shr1(&b);
    }
    *sticky = a.n != 0;
    return q;
}

static float from_bits(unsigned int bits)
{
    union fbits b;
    b.u = bits;
    return b.f;
}

// The float nearest num/den (both nonzero), rounded to nearest with ties to even.
static float nearest(const struct big* num, const struct big* den, int extra_sticky, int* range)
{
    int s = 25 - (big_bits(num) - big_bits(den));   // num/den x 2^s is in [2^24, 2^26)
    int sticky;
    unsigned int q = scaled_quotient(num, den, s, &sticky);
    sticky |= extra_sticky;
    if (q >= (1u << 25))
    {
        sticky |= (int)(q & 1u);
        q >>= 1;
        s--;
    }
    int biased = 24 - s + 127;
    if (biased >= 255)
    {
        *range = 1;
        return from_bits(0x7F800000u);
    }
    if (biased >= 1)
    {
        unsigned int m = q >> 1;
        if ((q & 1u) && (sticky || (m & 1u)))
            m++;
        if (m == (1u << 24))
        {
            m >>= 1;
            biased++;
            if (biased >= 255)
            {
                *range = 1;
                return from_bits(0x7F800000u);
            }
        }
        return from_bits(((unsigned int)biased << 23) | (m & 0x7FFFFFu));
    }
    // Below the normal range: count in units of 2^-149 (q in half units).
    q = scaled_quotient(num, den, 150, &sticky);
    sticky |= extra_sticky;
    unsigned int m = q >> 1;
    int inexact = (q & 1u) || sticky;
    if ((q & 1u) && (sticky || (m & 1u)))
        m++;
    if (inexact && m < 0x800000u)
        *range = 1;                                  // underflow: a subnormal (or zero) that is not exact
    return from_bits(m);                             // 2^23 is the smallest normal, which the pattern spells
}

float __fconv_decimal(const char* digits, int n, int sticky, int exp10, int* range)
{
    while (n > 0 && digits[0] == '0')
    {
        digits++;
        n--;
    }
    if (n == 0)
        return 0.0f;
    if (n + exp10 > 40)
    {
        *range = 1;                                  // at least 10^40: past the largest float
        return from_bits(0x7F800000u);
    }
    if (n + exp10 < -46)
    {
        *range = 1;                                  // below 10^-46: nearer 0 than the smallest subnormal
        return 0.0f;
    }
    struct big num, den;
    big_set(&num, 0);
    for (int i = 0; i < n && i < FCONV_MAX_DIGITS; i++)
    {
        big_mul_small(&num, 10u);
        big_add_small(&num, (unsigned int)(digits[i] - '0'));
    }
    if (n > FCONV_MAX_DIGITS)
    {
        for (int i = FCONV_MAX_DIGITS; i < n; i++)
            if (digits[i] != '0')
                sticky = 1;
        exp10 += n - FCONV_MAX_DIGITS;
    }
    if (sticky)
    {
        big_mul_small(&num, 10u);                    // one more digit, 1, for "more than this, less than the next"
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

float __fconv_binary(unsigned long long h, int sticky, int exp2, int* range)
{
    if (h == 0)
        return 0.0f;
    int top = 63 - (h >> 32 != 0 ? __builtin_clz((unsigned int)(h >> 32)) : 32 + __builtin_clz((unsigned int)h));
    if (top + exp2 > 128)
    {
        *range = 1;
        return from_bits(0x7F800000u);
    }
    if (top + exp2 < -151)
    {
        *range = 1;
        return 0.0f;
    }
    struct big num, den;
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
