// The error function and the gamma function: erf, erfc, tgamma, lgamma. Apart from math.c and math_ext.c, so a
// program that uses none of them does not carry their tables. The polynomials are Chebyshev fits made for float
// (see the comments), each good to a unit or two in the last place of its own range; include/math.h has how
// close the functions come.
#include "math_priv.h"

#include "math.h"
#include "errno.h"

int signgam;

// ---- erf and erfc ----

// erf(x) / x as a polynomial in x^2 on |x| <= 1.
static const float erf_small[6] = {
    1.12837911f, -0.376123428f, 0.112803169f, -0.0267150551f, 0.00492176181f, -0.000564805989f };

// erfc(x) * x * e^(x^2) as polynomials in t = 1/x on [1, 2], [2, 4] and [4, 10.1]; it tends to 1/sqrt(pi).
static const float erfc_near[10] = {
    0.563349664f, 0.0106451949f, -0.335803539f, 0.113872699f, 0.486136883f, -0.913868725f, 0.832263470f,
    -0.446304381f, 0.135312781f, -0.0180204362f };
static const float erfc_mid[10] = {
    0.564223289f, -0.000956268574f, -0.270108044f, -0.0865474567f, 0.812433600f, -1.07370079f, 0.430149794f,
    0.416131765f, -0.562096477f, 0.200309083f };
static const float erfc_far[9] = {
    0.564189553f, 0.00000231218792f, -0.282174021f, 0.00154918048f, 0.403929234f, 0.157707870f, -1.91678953f,
    2.95934606f, -1.64824021f };

// erfc(0.75 + u) on |u| <= 0.25: below 1, 1 - erf would cancel the digits erfc needs.
static const float erfc_half[8] = {
    0.288844377f, -0.642931044f, 0.482198149f, -0.0267885569f, -0.150675640f, 0.0532236919f, 0.0265828706f,
    -0.0179458465f };

static float horner(const float* c, int n, float x)
{
    float p = c[n - 1];
    for (int i = n - 2; i >= 0; i--)
        p = p * x + c[i];
    return p;
}

static float erf_below_one(float x)
{
    return x * horner(erf_small, 6, x * x);
}

// e^(-x^2) without the rounding of x*x: x = hi + lo with hi of 12 bits, so hi*hi is exact and
// x^2 = hi^2 + (x - hi)(x + hi) with the second part small.
static float exp_minus_square(float x)
{
    float hi = float_from_bits(float_bits(x) & 0xFFFFF000u);
    return exp(-hi * hi) * exp(-(x - hi) * (x + hi));
}

// erfc(x) for 1 <= x < 10.1.
static float erfc_tail(float x)
{
    float t = 1.0f / x;
    float g = x < 2.0f ? horner(erfc_near, 10, t) : x < 4.0f ? horner(erfc_mid, 10, t) : horner(erfc_far, 9, t);
    return exp_minus_square(x) * g * t;
}

float erf(float x)
{
    if (isnan(x))
        return x;
    float ax = fabs(x);
    if (ax < 1.0f)
        return erf_below_one(x);                     // keeps the sign of a zero
    float y = ax >= 4.0f ? 1.0f : 1.0f - erfc_tail(ax);   // from 4 on erfc is below half a unit of 1
    return copysign(y, x);
}

float erfc(float x)
{
    if (isnan(x))
        return x;
    if (x < 1.0f)
    {
        if (x >= 0.5f)
            return horner(erfc_half, 8, x - 0.75f);      // x - 0.75 is exact
        if (x > -1.0f)
            return 1.0f - erf_below_one(x);
        if (x <= -10.1f)
            return 2.0f;                             // 2 - (a tail that underflowed): exactly 2, and no ERANGE
        return 2.0f - erfc(-x);
    }
    if (x >= 10.1f)
    {
        if (isfinite(x))
            errno = ERANGE;                          // below the smallest float (and exact for +inf)
        return 0.0f;
    }
    return erfc_tail(x);
}

// ---- gamma ----

// gamma(1 + f) on 0 <= f <= 1. The constant term is the fit's 0.99999994 made 1, so gamma of every whole
// number is exact (a factorial, multiplied out in exact factors).
static const float gamma_unit[10] = {
    1.0f, -0.577206790f, 0.988756001f, -0.903452754f, 0.953478396f, -0.863298237f, 0.668840647f,
    -0.382352650f, 0.138283432f, -0.0230480302f };

static float gamma1p(float f)
{
    return horner(gamma_unit, 10, f);
}

// lgamma(1 + f) = f (f - 1) h(f) on 0 <= f <= 1: the zeros at 1 and 2 come out exactly, and the value near them
// keeps its relative precision.
static const float lgamma_unit[10] = {
    0.577215672f, -0.245250866f, 0.155417189f, -0.114918396f, 0.0906506479f, -0.0707246065f, 0.0490537919f,
    -0.0263120010f, 0.00914038625f, -0.00148747594f };

static float lgamma1p(float f)
{
    if (f == 0.0f)
        return 0.0f;                                 // +0, not the -0 of 0 * (0 - 1)
    return f * (f - 1.0f) * horner(lgamma_unit, 10, f);
}

// (f + 1)(f + 2) ... (f + n - 1) * g, with the product kept in a 32-bit integer significand and an exponent, so the
// thirty-odd factors of gamma(35) cost no more than a rounding at the end. Every factor is an exact float >= 1.
static float rising_product(float f, int n, float g)
{
    unsigned int m = 0x80000000u;                    // 1, as m * 2^e
    int e = -31;
    for (int k = 1; k < n; k++)
    {
        unsigned int bits = float_bits(f + (float)k);
        unsigned long long p = (unsigned long long)m * ((bits & 0x7FFFFFu) | 0x800000u);
        e += (int)((bits >> 23) & 255u) - 150;
        int shift = 0;
        while ((p >> shift) >= 0x100000000ull)
            shift++;
        if (shift > 0)
            p = (p + (1ull << (shift - 1))) >> shift;    // to nearest
        if (p >= 0x100000000ull)
        {
            p >>= 1;                                     // the rounding carried out of the top
            shift++;
        }
        m = (unsigned int)p;
        e += shift;
    }
    // m has 32 bits: its top 24, rounded, as a float, then g and the exponent.
    unsigned int top = (unsigned int)(((unsigned long long)m + 0x80u) >> 8);   // m near 2^32 must not wrap
    int extra = 8;
    if (top >= 0x1000000u)
    {
        top >>= 1;
        extra++;
    }
    return __scale2((float)top * g, e + extra);
}

// sin(pi x) with x reduced exactly first, so it is 0 at the integers and right near them.
static float sin_pi(float x)
{
    float r = x - 2.0f * floor(x * 0.5f);            // [0, 2), exact
    float sign = 1.0f;
    if (r >= 1.0f)
    {
        r -= 1.0f;
        sign = -1.0f;
    }
    if (r > 0.5f)
        r = 1.0f - r;
    return sign * sin(PI * r);
}

float tgamma(float x)
{
    if (isnan(x))
        return x;
    if (isinf(x))
    {
        if (x > 0.0f)
            return x;
        errno = EDOM;
        return NAN;
    }
    if (x == 0.0f)
    {
        errno = ERANGE;                              // a pole, on the side of the zero's sign
        return copysign(INFINITY, x);
    }
    if (x < 0.0f)
    {
        if (x == floor(x))
        {
            errno = EDOM;
            return NAN;
        }
        if (x < -42.0f)
        {
            errno = ERANGE;                          // below the smallest float, with the sign it would have
            return sin_pi(x) < 0.0f ? -0.0f : 0.0f;
        }
        // gamma(x) = gamma(x + n) / (x (x + 1) ... (x + n - 1)), with x + n in (0, 1); every x + k is exact. The
        // divisions go from the smallest factor up, so nothing overflows on the way.
        int n = (int)(-x) + 1;
        float y = x + (float)n;
        float g = gamma1p(y) / y;
        for (int k = n - 1; k >= 0; k--)
            g /= x + (float)k;
        if (g == 0.0f)
            errno = ERANGE;
        return g;
    }
    if (x > 35.05f)
    {
        errno = ERANGE;
        return INFINITY;
    }
    if (x < 1.0f)
    {
        float g = gamma1p(x) / x;                    // gamma(x) = gamma(x + 1) / x
        if (isinf(g))
            errno = ERANGE;                          // x so small that 1/x overflows
        return g;
    }
    int n = (int)x;
    float f = x - (float)n;                          // exact
    float g = rising_product(f, n, gamma1p(f));      // exact factors: x = n + f has room for each
    if (isinf(g))
        errno = ERANGE;
    return g;
}

float lgamma(float x)
{
    signgam = 1;
    if (isnan(x))
        return x;
    if (isinf(x))
        return INFINITY;
    if (x <= 0.0f && x == floor(x))
    {
        errno = ERANGE;                              // a pole
        return INFINITY;
    }
    if (x > 0.0f && x < 1.0f)
        return lgamma1p(x) - log(x);                 // lgamma(x) = lgamma(1 + x) - log x: both terms positive
    if (x >= 1.0f && x < 2.0f)
        return lgamma1p(x - 1.0f);                   // x - 1 is exact
    if (x >= 2.0f && x < 3.0f)
        return log1p(x - 2.0f) + lgamma1p(x - 2.0f); // lgamma(x) = log(x - 1) + lgamma(x - 1)
    if (x >= 3.0f && x < 35.0f)
        return log(tgamma(x));
    if (x >= 35.0f)
    {
        // Stirling: (x - 1/2) ln x - x + ln sqrt(2 pi) + 1/(12x) - 1/(360x^3) + 1/(1260x^5).
        float lx = log(x);
        float t = 1.0f / x, t2 = t * t;
        float series = t * (1.0f / 12.0f - t2 * (1.0f / 360.0f - t2 * (1.0f / 1260.0f)));
        float y = x * (lx - 1.0f) - 0.5f * lx + 0.918938533f + series;
        if (isinf(y))
            errno = ERANGE;
        return y;
    }
    if (x > -34.0f)
    {
        float g = tgamma(x);
        if (g < 0.0f)
            signgam = -1;
        return log(fabs(g));
    }
    // Reflection: gamma(x) gamma(1 - x) = pi / sin(pi x), and gamma(1 - x) > 0.
    float s = sin_pi(x);
    float rest = lgamma(1.0f - x);                   // (which sets signgam: set it after)
    signgam = s < 0.0f ? -1 : 1;
    return log(PI / fabs(s)) - rest;
}
