#include "math_priv.h"

#include "math.h"
#include "errno.h"

// Software float functions. The single-instruction operations (fabs, sqrt, floor, ceil, round,
// trunc, fmin, fmax, copysign, fma, rcp, rsqrt) live in asm/math_ops.casm, and fmod in src/fmod.c;
// everything here is built on those plus the ordinary + - * / operators, and on
// float_bits/float_from_bits to take a float apart. The rarer functions (hyperbolics, cbrt, hypot,
// ldexp, ...) are in math_ext.c so a program that only needs sin and sqrt does not carry them.
//
// A float division by zero does not fail: it sets the Trap flag and leaves the destination unchanged.
// So every division below is guarded, and the ways to reach a zero divisor are handled first.

// C's round(): halfway cases go AWAY from zero. (The `fround` instruction rounds them to even: that is
// rint(), in asm/math_ops.casm.) Written as trunc plus a correction, not floor(x + 0.5), which is wrong
// for 0.49999997f.
float round(float x)
{
    float t = trunc(x);
    if (fabs(x - t) >= 0.5f)
        t += copysign(1.0f, x);
    return t;
}

// ---- argument reduction for the trigonometric functions ----

// 2/pi, the first 256 bits after the binary point: enough for any float, the largest being below 2^128.
static const unsigned int two_over_pi[8] = {
    0xA2F9836Eu, 0x4E441529u, 0xFC2757D1u, 0xF534DDC0u, 0xDB629599u, 0x3C439041u, 0xFE5163ABu, 0xDEBBC561u };

// 32 bits of 2/pi from bit `b` after the point (bit 0 is worth 1/2); the bits before the point are 0.
static unsigned int two_over_pi_window(int b)
{
    int w = b >= 0 ? b / 32 : -((31 - b) / 32);
    int s = b - w * 32;
    unsigned int a = w >= 0 && w < 8 ? two_over_pi[w] : 0u;
    unsigned int c = w + 1 >= 0 && w + 1 < 8 ? two_over_pi[w + 1] : 0u;
    return s == 0 ? a : (a << s) | (c >> (32 - s));
}

// Payne and Hanek's reduction, for |x| too large for Cody and Waite: x * 2/pi worked out in integers from as many
// bits of 2/pi as the fraction needs, so r keeps every digit whatever the size of x. x is m * 2^e with m the
// 24-bit significand; the bits of 2/pi that m * 2^e would carry past 4 cannot change the quadrant and are skipped,
// and the next 96 give the product's fraction to 94 bits - more than the worst float needs (x a hair from a
// multiple of pi/2 cancels about 30 of them).
static float reduce_large(float ax, int* quad)
{
    unsigned int bits = float_bits(ax);
    unsigned long long m = (bits & 0x7FFFFFu) | 0x800000u;
    int e = (int)((bits >> 23) & 255u) - 150;
    int from = e - 2;
    unsigned long long p2 = m * two_over_pi_window(from + 64);
    unsigned long long p1 = m * two_over_pi_window(from + 32) + (p2 >> 32);
    unsigned long long p0 = m * two_over_pi_window(from) + (p1 >> 32);
    // The product is p0 : p1 : p2 in 32-bit limbs; its point is 94 bits up, in p0.
    unsigned int limb2 = (unsigned int)p0;
    unsigned long long fraction = ((unsigned long long)(limb2 & 0x3FFFFFFFu) << 34) |
                                  ((p1 & 0xFFFFFFFFull) << 2) | ((p2 & 0xFFFFFFFFull) >> 30);
    int q = (int)(limb2 >> 30);
    // To the nearest quadrant: a fraction of a half or more is the next one, from below.
    q += (int)(fraction >> 63);
    long long r = (long long)fraction;                 // the fraction, now in [-1/2, 1/2), times 2^64
    *quad = q & 3;
    int negative = r < 0;
    unsigned long long a = negative ? (unsigned long long)(-r) : (unsigned long long)r;
    if (a == 0)
        return 0.0f;
    int shift = 0;
    while ((a >> 63) == 0)
    {
        a <<= 1;
        shift++;
    }
    // r * pi/2 = (a / 2^64) * 2^-shift * pi/2: the top 32 bits of a times pi/2 in 32 bits (0xC90FDAA2 = pi/2 * 2^31).
    unsigned long long product = (a >> 32) * 0xC90FDAA2ull;
    float y = __scale2((float)(unsigned int)(product >> 32), -31 - shift);
    return negative ? -y : y;
}

// x = n * (pi/2) + r with |r| <= pi/4 (a hair more); *quad = n mod 4. Up to 6000, pi/2 is subtracted in three
// exact-product pieces (Cody and Waite), which keeps r right to its last digit; past it, reduce_large.
static float reduce_pio2(float x, int* quad)
{
    float ax = fabs(x);
    if (ax > 6000.0f)
    {
        float r = reduce_large(ax, quad);
        if (x < 0.0f)
        {
            *quad = (4 - *quad) & 3;
            r = -r;
        }
        return r;
    }
    float nf = floor(x * TWO_OVER_PI + 0.5f);
    *quad = ((int)nf) & 3;
    return ((x - nf * PIO2_1) - nf * PIO2_2) - nf * PIO2_3;
}

// sin, cos and tan of r, |r| <= pi/4: Cephes' minimax polynomials for float, which reach the float's last digit
// with three terms (sin, cos) and six (tan) where the Taylor series takes four and five and still needs a division
// for tan.
static float sin_kernel(float r)
{
    if (r == 0.0f)
        return r;                       // keeps -0: r + r*z*p would add +0 to it and lose the sign
    float z = r * r;
    float p = -1.9515295891e-4f;
    p = p * z + 8.3321608736e-3f;
    p = p * z - 1.6666654611e-1f;
    return r + r * z * p;
}

static float cos_kernel(float r)
{
    float z = r * r;
    float p = 2.443315711809948e-5f;
    p = p * z - 1.388731625493765e-3f;
    p = p * z + 4.166664568298827e-2f;
    return 1.0f - 0.5f * z + z * z * p;
}

static float tan_kernel(float r)
{
    float z = r * r;
    if (fabs(r) < 1.0e-4f)
        return r;
    float p = 9.38540185543e-3f;
    p = p * z + 3.11992232697e-3f;
    p = p * z + 2.44301354525e-2f;
    p = p * z + 5.34112807005e-2f;
    p = p * z + 1.33387994085e-1f;
    p = p * z + 3.33331568548e-1f;
    return r + r * z * p;
}

// NaN in, NaN out; an infinite argument has no sine: EDOM.
static int trig_domain(float x)
{
    if (isnan(x))
        return 1;
    if (isinf(x))
    {
        errno = EDOM;
        return 1;
    }
    return 0;
}

float sin(float x)
{
    if (trig_domain(x))
        return NAN;
    int q;
    float r = reduce_pio2(x, &q);
    if (q == 0) return sin_kernel(r);
    if (q == 1) return cos_kernel(r);
    if (q == 2) return -sin_kernel(r);
    return -cos_kernel(r);
}

float cos(float x)
{
    if (trig_domain(x))
        return NAN;
    int q;
    float r = reduce_pio2(x, &q);
    if (q == 0) return cos_kernel(r);
    if (q == 1) return -sin_kernel(r);
    if (q == 2) return -cos_kernel(r);
    return sin_kernel(r);
}

void sincos(float x, float* s, float* c)
{
    if (trig_domain(x))
    {
        *s = NAN;
        *c = NAN;
        return;
    }
    int q;
    float r = reduce_pio2(x, &q);
    float sk = sin_kernel(r);
    float ck = cos_kernel(r);
    if (q == 0)      { *s = sk;  *c = ck; }
    else if (q == 1) { *s = ck;  *c = -sk; }
    else if (q == 2) { *s = -sk; *c = -ck; }
    else             { *s = -ck; *c = sk; }
}

float tan(float x)
{
    if (trig_domain(x))
        return NAN;
    int q;
    float r = reduce_pio2(x, &q);
    float t = tan_kernel(r);
    if ((q & 1) == 0)
        return t;
    if (t == 0.0f)                          // tan(r + pi/2) = -1/tan(r): a pole no float reaches, but never divide by zero
    {
        errno = ERANGE;
        return copysign(INFINITY, -t);
    }
    return -1.0f / t;
}

// ---- inverse trigonometric ----

float atan(float x)
{
    if (isnan(x))
        return x;
    int neg = 0, big = 0, more = 0;
    if (x < 0.0f) { x = -x; neg = 1; }
    if (x > 1.0f) { x = 1.0f / x; big = 1; }   // -> [0, 1]; x may be infinite, 1/inf = 0
    if (x > TAN_PI_8)                          // -> [0, tan(pi/8)]
    {
        x = (x - 1.0f) / (x + 1.0f);
        more = 1;
    }

    // atan(x) = x * P(x^2), P the Taylor polynomial to 1/17.
    float xx = x * x;
    float p = 1.0f / 17.0f;
    p = p * xx - 1.0f / 15.0f;
    p = p * xx + 1.0f / 13.0f;
    p = p * xx - 1.0f / 11.0f;
    p = p * xx + 1.0f / 9.0f;
    p = p * xx - 1.0f / 7.0f;
    p = p * xx + 0.2f;
    p = p * xx - 1.0f / 3.0f;
    p = p * xx + 1.0f;
    float r = x * p;

    if (more) r = r + PI_4;
    if (big)  r = HALF_PI - r;
    if (neg)  r = -r;
    return r;
}

float atan2(float y, float x)
{
    if (isnan(x) || isnan(y))
        return x + y;
    if (y == 0.0f)
    {
        // +-0 with x > 0 or x == +0 is +-0; with x < 0 or x == -0 it is +-pi
        if (x > 0.0f || (x == 0.0f && !signbit(x)))
            return y;
        return signbit(y) ? -PI : PI;
    }
    if (x == 0.0f)
        return y > 0.0f ? HALF_PI : -HALF_PI;
    if (isinf(x))
    {
        if (isinf(y))                           // the corners: +-pi/4 and +-3pi/4
        {
            float a = x > 0.0f ? PI_4 : 3.0f * PI_4;
            return y > 0.0f ? a : -a;
        }
        if (x > 0.0f)
            return copysign(0.0f, y);
        return y > 0.0f ? PI : -PI;
    }
    if (isinf(y))
        return y > 0.0f ? HALF_PI : -HALF_PI;

    float a = atan(y / x);                      // y / x may overflow to infinity: atan handles that
    if (x < 0.0f)
        a += (y >= 0.0f) ? PI : -PI;
    return a;
}

float asin(float x)
{
    if (isnan(x))
        return x;
    if (x > 1.0f || x < -1.0f)
    {
        errno = EDOM;
        return NAN;
    }
    return atan2(x, sqrt((1.0f - x) * (1.0f + x)));
}

float acos(float x)
{
    if (isnan(x))
        return x;
    if (x > 1.0f || x < -1.0f)
    {
        errno = EDOM;
        return NAN;
    }
    return atan2(sqrt((1.0f - x) * (1.0f + x)), x);
}

// ---- exponentials and logarithms ----

// exp(r) for |r| <= ln2/2: the Cephes expf polynomial, good to about one unit in the last place.
static float exp_kernel(float r)
{
    float z = r * r;
    float p = 1.9875691500e-4f;
    p = p * r + 1.3981999507e-3f;
    p = p * r + 8.3334519073e-3f;
    p = p * r + 4.1665795894e-2f;
    p = p * r + 1.6666665459e-1f;
    p = p * r + 5.0000001201e-1f;
    return p * z + r + 1.0f;
}

// exp(x) * 2^shift, for the callers (sinh, cosh) whose result is e^x / 2 and must not overflow on the way.
// exp(x) = 2^n * exp(r) with n = round(x / ln2) and r = x - n ln2, subtracted in two parts. x must be
// finite and within about +-104; no errno is set here.
float __exp_shift(float x, int shift)
{
    float nf = floor(x * LOG2E + 0.5f);
    float r = (x - nf * LN2_C1) - nf * LN2_C2;
    return __scale2(exp_kernel(r), (int)nf + shift);
}

float exp(float x)
{
    if (isnan(x))
        return x;
    if (x > 89.0f)
    {
        if (!isinf(x))
            errno = ERANGE;                // exp(+inf) is +inf without a range error
        return INFINITY;
    }
    if (x < -104.0f)
    {
        if (!isinf(x))
            errno = ERANGE;
        return 0.0f;
    }
    float result = __exp_shift(x, 0);
    if (isinf(result))
        errno = ERANGE;
    return result;
}

// 2^x: the integer part goes straight into the exponent, so only the fraction is approximated.
float exp2(float x)
{
    if (isnan(x))
        return x;
    if (x > 129.0f)
    {
        if (!isinf(x))
            errno = ERANGE;
        return INFINITY;
    }
    if (x < -151.0f)
    {
        if (!isinf(x))
            errno = ERANGE;
        return 0.0f;
    }
    float nf = floor(x + 0.5f);
    float result = __scale2(exp_kernel((x - nf) * LN2), (int)nf);
    if (isinf(result))
        errno = ERANGE;
    return result;
}

// x = m * 2^*k with m in [1/sqrt2, sqrt2), for a positive finite x (a subnormal is scaled up first).
static float split_pow2(float x, int* k)
{
    unsigned int u = float_bits(x);
    int e = (int)(u >> 23);
    int adjust = 0;
    if (e == 0)
    {
        x = x * 16777216.0f;                   // 2^24
        u = float_bits(x);
        e = (int)(u >> 23);
        adjust = -24;
    }
    float m = float_from_bits((u & 0x007FFFFFu) | 0x3F800000u);   // [1, 2)
    int k0 = e - 127 + adjust;
    if (m > SQRT2)
    {
        m = m * 0.5f;
        k0++;
    }
    *k = k0;
    return m;
}

// log(m) for m in [1/sqrt2, sqrt2]: 2 * atanh((m-1)/(m+1)), a series in y^2 with |y| <= 0.172.
static float log_kernel(float m)
{
    float y = (m - 1.0f) / (m + 1.0f);
    float yy = y * y;
    float q = 1.0f / 9.0f;
    q = q * yy + 1.0f / 7.0f;
    q = q * yy + 0.2f;
    q = q * yy + 1.0f / 3.0f;
    q = q * yy + 1.0f;
    return 2.0f * y * q;
}

// The cases every logarithm shares. Returns 1 and sets *out when x settles it, else 0.
static int log_special(float x, float* out)
{
    if (isnan(x)) { *out = x; return 1; }
    if (x < 0.0f) { errno = EDOM; *out = NAN; return 1; }
    if (x == 0.0f) { errno = ERANGE; *out = -INFINITY; return 1; }
    if (isinf(x)) { *out = x; return 1; }
    return 0;
}

float log(float x)
{
    float special;
    if (log_special(x, &special))
        return special;
    int k;
    float m = split_pow2(x, &k);
    float kf = (float)k;
    return (kf * LN2_C2 + log_kernel(m)) + kf * LN2_C1;
}

float log2(float x)
{
    float special;
    if (log_special(x, &special))
        return special;
    int k;
    float m = split_pow2(x, &k);
    return (float)k + log_kernel(m) * LOG2E;
}

float log10(float x)
{
    float special;
    if (log_special(x, &special))
        return special;
    int k;
    float m = split_pow2(x, &k);
    return (float)k * LOG10_2 + log_kernel(m) * LOG10E;
}

// ---- pow ----

// base^n for a small non-negative n by repeated squaring (a multiplication or two per bit).
static float power_int(float base, int n)
{
    float result = 1.0f;
    while (n > 0)
    {
        if (n & 1)
            result = result * base;
        base = base * base;
        n >>= 1;
    }
    return result;
}

// An odd integer? Every float of 2^24 or more is an even integer.
static int is_odd_integer(float y)
{
    if (fabs(y) >= 16777216.0f || trunc(y) != y)
        return 0;
    return ((int)y) & 1;
}

float pow(float x, float y)
{
    if (y == 0.0f || x == 1.0f)
        return 1.0f;                                    // even when the other one is NaN
    if (isnan(x) || isnan(y))
        return NAN;

    int y_is_integer = trunc(y) == y;
    int odd = is_odd_integer(y);

    if (isinf(y))
    {
        float ax = fabs(x);
        if (ax == 1.0f)
            return 1.0f;
        return ((ax > 1.0f) == (y > 0.0f)) ? INFINITY : 0.0f;
    }
    if (isinf(x))
    {
        if (y > 0.0f)
            return (x < 0.0f && odd) ? -INFINITY : INFINITY;
        return (x < 0.0f && odd) ? -0.0f : 0.0f;
    }
    if (x == 0.0f)
    {
        int negative_zero = signbit(x);
        if (y > 0.0f)
            return (negative_zero && odd) ? -0.0f : 0.0f;
        errno = ERANGE;                                 // 0 to a negative power: a pole
        return (negative_zero && odd) ? -INFINITY : INFINITY;
    }

    int negative = 0;
    if (x < 0.0f)
    {
        if (!y_is_integer)
        {
            errno = EDOM;                               // a negative base needs a whole exponent
            return NAN;
        }
        negative = odd;
        x = -x;
    }

    float r;
    if (y_is_integer && fabs(y) <= 32.0f)
    {
        int n = (int)fabs(y);
        r = power_int(x, n);
        if (y < 0.0f)
        {
            if (r == 0.0f || isinf(r))
                r = (r == 0.0f) ? INFINITY : 0.0f;      // x^-n underflows to inf, or overflows to 0
            else
                r = 1.0f / r;
        }
    }
    else
    {
        // The error of y*log(x) is multiplied into the result: about |y log x| * 6e-8 relative.
        r = exp(y * log(x));
    }
    if (isinf(r) || r == 0.0f)
        errno = ERANGE;
    return negative ? -r : r;
}
