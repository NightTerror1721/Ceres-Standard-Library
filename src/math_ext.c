#include "math_priv.h"

#include "math.h"
#include "errno.h"

// The less common math functions. Split from math.c so that a program that only needs sin, sqrt and
// pow does not carry them (an archive member is pulled in only when something asks for a symbol in it).
// As in math.c, no division here can have a zero divisor: a float division by zero does not fail, it
// sets the Trap flag and leaves the destination unchanged.

// ---- exponent handling ----

float ldexp(float x, int e)
{
    if (x == 0.0f || isinf(x) || isnan(x))
        return x;
    float r = __scale2(x, e);
    if (isinf(r) || r == 0.0f)
        errno = ERANGE;
    return r;
}

float frexp(float x, int* e)
{
    *e = 0;
    if (x == 0.0f || isinf(x) || isnan(x))
        return x;
    unsigned int u = float_bits(x);
    int adjust = 0;
    if (((u >> 23) & 255u) == 0)                   // subnormal: scale it into the normal range first
    {
        u = float_bits(x * 16777216.0f);           // 2^24
        adjust = -24;
    }
    *e = (int)((u >> 23) & 255u) - 126 + adjust;
    return float_from_bits((u & 0x807FFFFFu) | 0x3F000000u);   // exponent field 126: [0.5, 1)
}

float modf(float x, float* ip)
{
    if (isinf(x))
    {
        *ip = x;
        return copysign(0.0f, x);
    }
    if (isnan(x))
    {
        *ip = x;
        return x;
    }
    float whole = trunc(x);
    *ip = whole;
    return copysign(x - whole, x);                 // a whole x has a fraction of exactly +-0
}

// ---- rounding ----

// nearbyint and rint (ties to even) are the `fround` instruction, in asm/math_ops.casm.

int lround(float x)
{
    if (isnan(x))
    {
        errno = EDOM;
        return 0;
    }
    float r = round(x);
    if (r >= 2147483648.0f)
    {
        errno = ERANGE;
        return 2147483647;
    }
    if (r < -2147483648.0f)
    {
        errno = ERANGE;
        return -2147483647 - 1;
    }
    return (int)r;
}

int lrint(float x)
{
    if (isnan(x))
    {
        errno = EDOM;
        return 0;
    }
    float r = nearbyint(x);
    if (r >= 2147483648.0f)
    {
        errno = ERANGE;
        return 2147483647;
    }
    if (r < -2147483648.0f)
    {
        errno = ERANGE;
        return -2147483647 - 1;
    }
    return (int)r;
}

// The 64-bit forms. Every float of magnitude 2^23 and up is already a whole number, so the rounding
// is lround's and lrint's; only the range differs, and that is ll_from_rounded's: -2^63 is a float, and
// the only one at the bottom edge; 2^63 is the first one past the top.
static long long ll_from_rounded(float r)
{
    if (r >= 9223372036854775808.0f)
    {
        errno = ERANGE;
        return 9223372036854775807LL;
    }
    if (r < -9223372036854775808.0f)
    {
        errno = ERANGE;
        return -9223372036854775807LL - 1;
    }
    return (long long)r;
}

long long llround(float x)
{
    if (isnan(x))
    {
        errno = EDOM;
        return 0;
    }
    return ll_from_rounded(round(x));
}

long long llrint(float x)
{
    if (isnan(x))
    {
        errno = EDOM;
        return 0;
    }
    return ll_from_rounded(nearbyint(x));
}

// ---- simple arithmetic ----

float fdim(float x, float y)
{
    if (isnan(x) || isnan(y))
        return NAN;
    return x > y ? x - y : 0.0f;
}

float remainder(float x, float y)
{
    if (isnan(x) || isnan(y) || isinf(x) || y == 0.0f)
    {
        if (!isnan(x) && !isnan(y))
            errno = EDOM;
        return NAN;
    }
    if (isinf(y))
        return x;
    float ay = fabs(y);
    float r = fmod(fabs(x), ay);                   // 0 <= r < |y|
    float twice = r + r;
    if (r == 0.0f)
        return copysign(0.0f, x);                  // an exact multiple: zero with the sign of x
    if (twice > ay || (twice == ay && fmod(fabs(x), ay + ay) >= ay))
        r -= ay;                                   // nearer the next multiple, or halfway with an odd quotient
    return x < 0.0f ? -r : r;                      // r was found for |x|: mirror it for a negative x
}

float nan(const char* tag) { return NAN; }

float hypot(float x, float y)
{
    if (isinf(x) || isinf(y))
        return INFINITY;                           // an infinity wins even over a NaN
    if (isnan(x) || isnan(y))
        return NAN;
    float a = fabs(x);
    float b = fabs(y);
    if (a < b)
    {
        float t = a;
        a = b;
        b = t;
    }
    if (b == 0.0f)
        return a;
    float q = b / a;                               // a >= b > 0, so a is not zero
    float r = a * sqrt(1.0f + q * q);
    if (isinf(r))
        errno = ERANGE;
    return r;
}

// ---- roots ----

float cbrt(float x)
{
    if (x == 0.0f || isnan(x) || isinf(x))
        return x;
    float a = fabs(x);
    float scale = 1.0f;
    if (a < 1.17549435e-38f)                       // subnormal: 2^24 is a cube's worth of 2^8, undone below
    {
        a = a * 16777216.0f;
        scale = 1.0f / 256.0f;
    }
    // A first guess from the exponent (a third of it), then Newton's y = (2y + a/y^2) / 3, which
    // converges quadratically: 5% -> 0.2% -> 3e-6 -> full precision.
    float y = float_from_bits(float_bits(a) / 3u + 0x2A5137A0u);
    y = (2.0f * y + a / (y * y)) * (1.0f / 3.0f);
    y = (2.0f * y + a / (y * y)) * (1.0f / 3.0f);
    y = (2.0f * y + a / (y * y)) * (1.0f / 3.0f);
    y = y * scale;
    return copysign(y, x);
}

// ---- exp and log near their fixed points ----

float expm1(float x)
{
    if (isnan(x))
        return x;
    if (x > 88.7f || x < -18.0f)
        return exp(x) - 1.0f;                      // far from 0 there is nothing to lose (exp(-18) - 1 is -1)
    float ax = fabs(x);
    if (ax < LN2_HALF)
    {
        // the Taylor series to x^9/9!, without the leading 1, so nothing cancels
        float p = 1.0f / 362880.0f;
        p = p * x + 1.0f / 40320.0f;
        p = p * x + 1.0f / 5040.0f;
        p = p * x + 1.0f / 720.0f;
        p = p * x + 1.0f / 120.0f;
        p = p * x + 1.0f / 24.0f;
        p = p * x + 1.0f / 6.0f;
        p = p * x + 0.5f;
        return x + x * x * p;
    }
    return exp(x) - 1.0f;
}

float log1p(float x)
{
    if (isnan(x))
        return x;
    if (x < -1.0f)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == -1.0f)
    {
        errno = ERANGE;
        return -INFINITY;
    }
    if (isinf(x))
        return x;
    if (fabs(x) < 0.25f)
    {
        // log(1+x) = 2 atanh(x / (2+x)); |y| < 0.112 so the series is short and 1+x is never formed
        float y = x / (2.0f + x);
        float yy = y * y;
        float q = 1.0f / 9.0f;
        q = q * yy + 1.0f / 7.0f;
        q = q * yy + 0.2f;
        q = q * yy + 1.0f / 3.0f;
        q = q * yy + 1.0f;
        return 2.0f * y * q;
    }
    return log(1.0f + x);
}

// ---- hyperbolic ----

// e^ax / 2 for ax that would overflow e^ax itself but not e^ax / 2. The halving is done on the exponent,
// not by subtracting ln 2 from ax: that subtraction would cost the digits a float has at ax ~ 89.
static float half_exp(float ax)
{
    if (ax > 89.5f)
    {
        errno = ERANGE;
        return INFINITY;
    }
    float r = __exp_shift(ax, -1);
    if (isinf(r))
        errno = ERANGE;
    return r;
}

float sinh(float x)
{
    if (isnan(x) || isinf(x))
        return x;
    float ax = fabs(x);
    float r;
    if (ax < 1.0f)
    {
        float em1 = expm1(ax);                     // no cancellation for small x: sinh = (e^x - e^-x) / 2
        r = 0.5f * (em1 + em1 / (em1 + 1.0f));
    }
    else if (ax > 88.0f)
    {
        r = half_exp(ax);
    }
    else
    {
        float t = exp(ax);
        r = 0.5f * (t - 1.0f / t);
    }
    return copysign(r, x);
}

float cosh(float x)
{
    if (isnan(x))
        return x;
    if (isinf(x))
        return INFINITY;
    float ax = fabs(x);
    if (ax > 88.0f)
        return half_exp(ax);
    float t = exp(ax);
    return 0.5f * (t + 1.0f / t);
}

float tanh(float x)
{
    if (isnan(x))
        return x;
    float ax = fabs(x);
    if (ax > 9.0f)
        return copysign(1.0f, x);                  // 1 - tanh(9) < 3e-8: below a float's resolution
    float em1 = expm1(ax + ax);
    return copysign(em1 / (em1 + 2.0f), x);
}

float asinh(float x)
{
    if (isnan(x) || isinf(x))
        return x;
    float ax = fabs(x);
    float r;
    if (ax > 268435456.0f)                         // 2^28: x*x would overflow; asinh(x) ~ log(2x)
        r = log(ax) + LN2;
    else
        r = log1p(ax + ax * ax / (1.0f + sqrt(1.0f + ax * ax)));
    return copysign(r, x);
}

float acosh(float x)
{
    if (isnan(x))
        return x;
    if (x < 1.0f)
    {
        errno = EDOM;
        return NAN;
    }
    if (isinf(x))
        return x;
    if (x > 268435456.0f)
        return log(x) + LN2;
    float t = x - 1.0f;
    return log1p(t + sqrt(t * (x + 1.0f)));       // log(x + sqrt(x^2 - 1)), without forming x^2 - 1
}

float atanh(float x)
{
    if (isnan(x))
        return x;
    float ax = fabs(x);
    if (ax > 1.0f)
    {
        errno = EDOM;
        return NAN;
    }
    if (ax == 1.0f)
    {
        errno = ERANGE;
        return copysign(INFINITY, x);
    }
    return copysign(0.5f * log1p((ax + ax) / (1.0f - ax)), x);
}

// ---- the exponent, and the next float ----

int ilogb(float x)
{
    if (x == 0.0f || isnan(x))
    {
        errno = EDOM;
        return FP_ILOGB0;                                // FP_ILOGBNAN is the same number
    }
    if (isinf(x))
    {
        errno = EDOM;
        return 2147483647;
    }
    int e;
    frexp(x, &e);                                        // 0.5 <= |m| < 1: one below it is floor(log2|x|)
    return e - 1;
}

float logb(float x)
{
    if (isnan(x))
        return x;
    if (isinf(x))
        return INFINITY;
    if (x == 0.0f)
    {
        errno = ERANGE;                                  // a pole
        return -INFINITY;
    }
    return (float)ilogb(x);
}

float nextafter(float x, float y)
{
    if (isnan(x) || isnan(y))
        return x + y;
    if (x == y)
        return y;                                        // y, so nextafter(0, -0) is -0
    float r;
    if (x == 0.0f)
        r = copysign(float_from_bits(1u), y);            // the smallest subnormal, towards y
    else
    {
        unsigned int bits = float_bits(x);
        bits = ((x < y) == (x > 0.0f)) ? bits + 1u : bits - 1u;   // away from zero, or towards it
        r = float_from_bits(bits);
    }
    if (isinf(r) || !isnormal(r))
        errno = ERANGE;                                  // overflowed, or subnormal or zero
    return r;
}
