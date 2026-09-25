// IEEE 754 binary64 in software. See ceres/f64.h.
//
// Every operation works on a value taken apart into its sign, its biased exponent and a significand `m` with its
// leading 1 at bit 63 - the number is m / 2^63 * 2^(biased - 1023) - and puts the result back together through
// pack(), the one place that rounds. The eleven bits below the 53 a double keeps, and a sticky bit folded into the
// lowest, are what the rounding looks at.
#include "ceres/f64.h"
#include "math.h"

typedef unsigned long long u64;

#define SIGN_BIT   0x8000000000000000ull
#define FRAC_MASK  0x000FFFFFFFFFFFFFull
#define QUIET_BIT  0x0008000000000000ull

static int is_nan(u64 a) { return (a & ~SIGN_BIT) > F64_INFINITY; }
static int is_inf(u64 a) { return (a & ~SIGN_BIT) == F64_INFINITY; }
static int is_zero(u64 a) { return (a & ~SIGN_BIT) == 0; }

// A NaN operand, quieted, or the default NaN.
static u64 nan_of(u64 a, u64 b)
{
    if (is_nan(a))
        return a | QUIET_BIT;
    if (is_nan(b))
        return b | QUIET_BIT;
    return F64_NAN;
}

static int leading_zeros(u64 m)
{
    int n = 0;
    if ((m >> 32) == 0) { n += 32; m <<= 32; }
    if ((m >> 48) == 0) { n += 16; m <<= 16; }
    if ((m >> 56) == 0) { n += 8; m <<= 8; }
    if ((m >> 60) == 0) { n += 4; m <<= 4; }
    if ((m >> 62) == 0) { n += 2; m <<= 2; }
    if ((m >> 63) == 0) { n += 1; }
    return n;
}

// m shifted right by n, with every bit shifted out folded into the lowest: what rounding needs to know of them.
static u64 shift_sticky(u64 m, int n)
{
    if (n <= 0)
        return m;
    if (n >= 64)
        return m != 0;
    return (m >> n) | ((m << (64 - n)) != 0);
}

// A finite, non-zero a: its biased exponent and its significand with the leading 1 at bit 63.
static int unpack(u64 a, u64* m)
{
    int biased = (int)((a >> 52) & 0x7FFu);
    u64 frac = a & FRAC_MASK;
    if (biased == 0)
    {
        int z = leading_zeros(frac << 11);
        *m = (frac << 11) << z;                      // a subnormal, brought to the normal form
        return 1 - z;
    }
    *m = (frac | 0x0010000000000000ull) << 11;
    return biased;
}

// sign, biased exponent and a significand with its leading 1 at bit 63 (or m == 0), rounded to nearest-even.
static u64 pack(u64 sign, int biased, u64 m)
{
    if (m == 0)
        return sign;
    if (biased >= 2047)
        return sign | F64_INFINITY;
    if (biased < 1)
    {
        m = shift_sticky(m, 1 - biased);             // a subnormal: fewer bits of it are kept
        biased = 1;
    }
    u64 sig = m >> 11;
    unsigned int rest = (unsigned int)(m & 0x7FFu);
    if (rest > 0x400u || (rest == 0x400u && (sig & 1u)))
        sig++;
    // The leading 1 of sig adds one to the exponent field: biased - 1 + 1. A subnormal's sig has none (and gets
    // one if the rounding carried into it); a rounding that carries out of 53 bits adds its one more.
    u64 bits = ((u64)(biased - 1) << 52) + sig;
    if ((bits >> 52) >= 2047u)
        return sign | F64_INFINITY;
    return sign | bits;
}

// ---- add and subtract ----

f64 __f64_add(f64 a, f64 b)
{
    if (is_nan(a) || is_nan(b))
        return nan_of(a, b);
    if (is_inf(a) || is_inf(b))
    {
        if (is_inf(a) && is_inf(b) && ((a ^ b) & SIGN_BIT))
            return F64_NAN;                          // inf - inf
        return is_inf(a) ? a : b;
    }
    if (is_zero(a) || is_zero(b))
    {
        if (is_zero(a) && is_zero(b))
            return a & b & SIGN_BIT;                 // -0 only when both are
        return is_zero(a) ? b : a;
    }
    // |a| >= |b| from here on.
    if ((a & ~SIGN_BIT) < (b & ~SIGN_BIT))
    {
        u64 t = a;
        a = b;
        b = t;
    }
    u64 ma, mb;
    int ea = unpack(a, &ma);
    int eb = unpack(b, &mb);
    // One bit of headroom for the carry of a sum; b lined up under a, with what falls off kept sticky.
    ma >>= 1;
    mb = shift_sticky(mb >> 1, ea - eb);
    u64 s = ((a ^ b) & SIGN_BIT) ? ma - mb : ma + mb;
    if (s == 0)
        return 0;                                    // x - x is +0
    int z = leading_zeros(s);
    return pack(a & SIGN_BIT, ea + 1 - z, s << z);
}

f64 __f64_sub(f64 a, f64 b)
{
    return __f64_add(a, is_nan(b) ? b : b ^ SIGN_BIT);
}

f64 __f64_neg(f64 a)
{
    return a ^ SIGN_BIT;
}

// ---- multiply ----

// The 128-bit product of a and b: its high half, and the low half in *low.
static u64 mul128(u64 a, u64 b, u64* low)
{
    u64 al = a & 0xFFFFFFFFull, ah = a >> 32;
    u64 bl = b & 0xFFFFFFFFull, bh = b >> 32;
    u64 ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    u64 mid = (ll >> 32) + (lh & 0xFFFFFFFFull) + (hl & 0xFFFFFFFFull);
    *low = (mid << 32) | (ll & 0xFFFFFFFFull);
    return hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
}

f64 __f64_mul(f64 a, f64 b)
{
    u64 sign = (a ^ b) & SIGN_BIT;
    if (is_nan(a) || is_nan(b))
        return nan_of(a, b);
    if (is_inf(a) || is_inf(b))
    {
        if (is_zero(a) || is_zero(b))
            return F64_NAN;                          // inf * 0
        return sign | F64_INFINITY;
    }
    if (is_zero(a) || is_zero(b))
        return sign;
    u64 ma, mb, low;
    int e = unpack(a, &ma) + unpack(b, &mb) - 1023;
    u64 high = mul128(ma, mb, &low);                 // (ma / 2^63) (mb / 2^63) = high / 2^62 + ...: in [1, 4)
    if ((high >> 63) == 0)
    {
        high = (high << 1) | (low >> 63);
        low <<= 1;
    }
    else
        e++;
    return pack(sign, e, high | (low != 0));
}

// ---- divide ----

f64 __f64_div(f64 a, f64 b)
{
    u64 sign = (a ^ b) & SIGN_BIT;
    if (is_nan(a) || is_nan(b))
        return nan_of(a, b);
    if (is_inf(a))
        return is_inf(b) ? F64_NAN : (sign | F64_INFINITY);
    if (is_inf(b))
        return sign;
    if (is_zero(b))
        return is_zero(a) ? F64_NAN : (sign | F64_INFINITY);   // 0/0, and x/0
    if (is_zero(a))
        return sign;
    u64 ma, mb;
    int e = unpack(a, &ma) - unpack(b, &mb) + 1023;
    u64 r = ma >> 1, d = mb >> 1;                    // bit 62: room to shift the remainder left
    if (r < d)
    {
        r <<= 1;
        e--;
    }
    // d <= r < 2d: 64 bits of the quotient, the first of them 1, and whether anything remained.
    u64 q = 0;
    for (int i = 0; i < 64; i++)
    {
        q <<= 1;
        if (r >= d)
        {
            r -= d;
            q |= 1u;
        }
        r <<= 1;
    }
    return pack(sign, e, q | (r != 0));
}

// ---- square root ----

f64 __f64_sqrt(f64 a)
{
    if (is_nan(a))
        return a | QUIET_BIT;
    if (is_zero(a))
        return a;                                    // sqrt(-0) is -0
    if (a & SIGN_BIT)
        return F64_NAN;
    if (is_inf(a))
        return a;
    u64 m;
    int e = unpack(a, &m) - 1023;                    // the number is m / 2^63 * 2^e
    // The radicand as a 128-bit integer n = m * 2^63 (e even) or m * 2^64 (e odd, e made even): its root has
    // its leading 1 at bit 63. Found a bit at a time; what is left over says whether it was exact.
    u64 n_high, n_low;
    if (e & 1)
    {
        n_high = m;
        n_low = 0;
        e -= 1;
    }
    else
    {
        n_high = m >> 1;
        n_low = m << 63;
    }
    u64 root = 0;
    for (int bit = 63; bit >= 0; bit--)
    {
        u64 trial = root | (1ull << bit);
        u64 low;
        u64 high = mul128(trial, trial, &low);
        if (high < n_high || (high == n_high && low <= n_low))
            root = trial;
    }
    u64 low;
    u64 high = mul128(root, root, &low);
    int exact = high == n_high && low == n_low;
    return pack(0, e / 2 + 1023, root | (exact ? 0u : 1u));
}

// ---- compare ----

int __f64_cmp(f64 a, f64 b)
{
    if (is_nan(a) || is_nan(b))
        return 2;
    if (is_zero(a) && is_zero(b))
        return 0;
    int sa = (a & SIGN_BIT) != 0, sb = (b & SIGN_BIT) != 0;
    if (sa != sb)
        return sa ? -1 : 1;
    if (a == b)
        return 0;
    int less = (a & ~SIGN_BIT) < (b & ~SIGN_BIT);    // by magnitude; a negative pair the other way round
    return less != sa ? -1 : 1;
}

// ---- conversions ----

f64 __f64_from_f32(float x)
{
    unsigned int bits = float_bits(x);
    u64 sign = (u64)(bits >> 31) << 63;
    unsigned int e = (bits >> 23) & 255u;
    u64 frac = bits & 0x7FFFFFu;
    if (e == 255u)
        return sign | F64_INFINITY | (frac << 29) | (frac != 0 ? QUIET_BIT : 0u);
    if (e == 0u)
    {
        if (frac == 0)
            return sign;
        int z = leading_zeros(frac << 40);           // a float subnormal is a double normal
        return pack(sign, (int)(1 - 127 + 1023) - z, (frac << 40) << z);
    }
    return sign | ((u64)(e - 127u + 1023u) << 52) | (frac << 29);
}

float __f64_to_f32(f64 a)
{
    unsigned int sign = (unsigned int)(a >> 63) << 31;
    if (is_nan(a))
        return float_from_bits(sign | 0x7FC00000u | (unsigned int)((a & FRAC_MASK) >> 29));
    if (is_inf(a))
        return float_from_bits(sign | 0x7F800000u);
    if (is_zero(a))
        return float_from_bits(sign);
    u64 m;
    int e = unpack(a, &m) - 1023 + 127;              // the float's biased exponent
    if (e >= 255)
        return float_from_bits(sign | 0x7F800000u);
    if (e < 1)
    {
        m = shift_sticky(m, 1 - e);
        e = 1;
    }
    unsigned int sig = (unsigned int)(m >> 40);      // 24 bits, the leading one at bit 23
    u64 rest = m & 0xFFFFFFFFFFull;
    if (rest > 0x8000000000ull || (rest == 0x8000000000ull && (sig & 1u)))
        sig++;
    unsigned int bits = ((unsigned int)(e - 1) << 23) + sig;
    if ((bits >> 23) >= 255u)
        return float_from_bits(sign | 0x7F800000u);
    return float_from_bits(sign | bits);
}

f64 __f64_from_u64(unsigned long long x)
{
    if (x == 0)
        return 0;
    int z = leading_zeros(x);
    return pack(0, 1023 + 63 - z, x << z);
}

f64 __f64_from_i64(long long x)
{
    u64 sign = x < 0 ? SIGN_BIT : 0;
    u64 magnitude = x < 0 ? 0ull - (u64)x : (u64)x;
    return sign | __f64_from_u64(magnitude);
}

f64 __f64_from_i32(int x)
{
    return __f64_from_i64(x);
}

f64 __f64_from_u32(unsigned int x)
{
    return __f64_from_u64(x);
}

// |a| truncated to an integer, when it is below 2^64; *big says when it is not.
static u64 truncated(f64 a, int* big)
{
    *big = 0;
    if (is_zero(a))
        return 0;
    u64 m;
    int e = unpack(a, &m) - 1023;                    // the number is m / 2^63 * 2^e
    if (e < 0)
        return 0;
    if (e > 63)
    {
        *big = 1;
        return 0;
    }
    return m >> (63 - e);
}

long long __f64_to_i64(f64 a)
{
    if (is_nan(a))
        return 0;
    int big;
    u64 v = truncated(a, &big);
    int negative = (a & SIGN_BIT) != 0;
    if (negative)
        return big || v > 0x8000000000000000ull ? (long long)0x8000000000000000ull : (long long)(0ull - v);
    return big || v > 0x7FFFFFFFFFFFFFFFull ? 0x7FFFFFFFFFFFFFFFll : (long long)v;
}

unsigned long long __f64_to_u64(f64 a)
{
    if (is_nan(a) || (a & SIGN_BIT))
        return 0;                                    // a negative number's nearest unsigned value
    int big;
    u64 v = truncated(a, &big);
    return big ? 0xFFFFFFFFFFFFFFFFull : v;
}

int __f64_to_i32(f64 a)
{
    long long v = __f64_to_i64(a);
    if (v > 2147483647LL)
        return 2147483647;
    if (v < -2147483647LL - 1)
        return -2147483647 - 1;
    return (int)v;
}

unsigned int __f64_to_u32(f64 a)
{
    unsigned long long v = __f64_to_u64(a);
    return v > 0xFFFFFFFFull ? 0xFFFFFFFFu : (unsigned int)v;
}
