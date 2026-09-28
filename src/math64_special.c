#include "math64_priv.h"

#include "math.h"
#include "errno.h"

// The error and gamma functions for doubles. erf is its Taylor series below 0.75, erfc a continued fraction from 1/2
// up (and each is 1 minus the other where that one is used), and e^(-x^2) - the factor both carry - is worked out from x^2 as a pair. lgamma is the series of
// lgamma(1 + f) (coefficients (-1)^k zeta(k) / k) near 1 and 2, the recurrence between, and Stirling's series from
// 12 up; tgamma is e^lgamma(1 + f) times the recurrence below 12, and Stirling's formula above it with x/e as a pair.
// Each is within a few units in the last place - except lgamma next to its zeros between the poles for x < 0, where it
// keeps about 1e-16 absolute. (signgam is defined with the float functions, in src/math_special.c.)

#define TWO_OVER_SQRT_PI 1.1283791670955126
#define ONE_OVER_SQRT_PI 0.5641895835477563
#define SQRT_2PI         2.5066282746310007
#define LN_SQRT_2PI      0.9189385332046728
#define LN_SQRT_2PI_LO   (-3.8782941580672414e-17)
#define LN_SQRT_2PI_LO   (-3.8782941580672414e-17)
#define EULER            0.5772156649015329
#define INV_E_HI         0.36787944117144233
#define INV_E_LO         (-1.2428753672788363e-17)

// ---- erf, erfc ---------------------------------------------------------------------------------------------

// e^(-x^2), with x^2 = h + l exactly: e^-h * (1 - l).
static double exp_minus_square(double x)
{
    double h, l;
    __two_product(x, x, &h, &l);
    return exp(-h) * (1.0 - l);
}

// (-1)^n / (n! (2n + 1)) for n = 0..19: erf(x) = 2/sqrt(pi) x sum of these times x^(2n).
static const double ERF_TAYLOR[20] = {
    1.0, -0.3333333333333333, 0.1, -0.023809523809523808, 0.004629629629629629, -0.0007575757575757576,
    0.00010683760683760684, -1.3227513227513228e-05, 1.4589169000933706e-06, -1.4503852223150468e-07,
    1.3122532963802806e-08, -1.0892221037148573e-09, 8.35070279514724e-11, -5.9477940136376354e-12,
    3.9554295164585257e-13, -2.466827010264457e-14, 1.4483264643598138e-15, -8.032735012415773e-17,
    4.221407288807088e-18, -2.107855191442136e-19 };

// erf(x) for |x| < 0.75: the Taylor series, summed from its smallest term up (x^2 is at most 0.5625, so twenty
// terms reach 2^-60).
static double erf_series(double x)
{
    double y = x * x;
    double p = ERF_TAYLOR[19];
    for (int n = 18; n >= 0; n--)
        p = p * y + ERF_TAYLOR[n];
    return TWO_OVER_SQRT_PI * (x * p);
}

// erfc(x) for x >= 1/2: e^(-x^2)/sqrt(pi) over the continued fraction x + (1/2)/(x + 1/(x + (3/2)/(x + ...))),
// evaluated from the bottom up - which keeps the rounding of each step from piling up the way a forward (Lentz)
// evaluation's does. 20 + 200/x^2 steps leave less than 2^-60: 820 at 1/2, 70 at 2, 22 from 10 up.
static double erfc_fraction(double x)
{
    int steps = 20 + (int)(200.0 / (x * x));
    double t = x;
    for (int n = steps; n >= 1; n--)
        t = x + (0.5 * n) / t;
    return exp_minus_square(x) * ONE_OVER_SQRT_PI / t;
}

double erf(double x)
{
    if (x != x)
        return x + x;
    if (isinf(x))
        return x > 0.0 ? 1.0 : -1.0;
    double ax = fabs(x);
    if (ax < 0x1p-28)
        return x + x * (TWO_OVER_SQRT_PI - 1.0);
    if (ax < 0.75)
        return erf_series(x);
    if (ax >= 6.0)
        return x > 0.0 ? 1.0 : -1.0;                     // erfc(6) is below 2^-55
    double r = 1.0 - erfc_fraction(ax);
    return x > 0.0 ? r : -r;
}

double erfc(double x)
{
    if (x != x)
        return x + x;
    if (isinf(x))
        return x > 0.0 ? 0.0 : 2.0;
    if (x < 0.5)
    {
        if (x > -0.75)
            return 1.0 - erf_series(x);
        if (x < -6.0)
            return 2.0;
        return 2.0 - erfc_fraction(-x);
    }
    if (x > 27.3)
    {
        errno = ERANGE;                                  // below the smallest double
        return 0.0;
    }
    double r = erfc_fraction(x);
    if (r == 0.0 || r < 0x1p-1022)
        errno = ERANGE;
    return r;
}

// ---- lgamma, tgamma ----------------------------------------------------------------------------------------

// (-1)^k zeta(k) / k for k = 2..59: lgamma(1 + f) = -gamma f + sum of these times f^k, for |f| <= 1/2.
static const double LG1P[58] = {
    0.8224670334241132, -0.40068563438653143, 0.27058080842778454,
    -0.20738555102867398, 0.1695571769974082, -0.1440498967688461,
    0.12550966952474304, -0.11133426586956469, 0.1000994575127818,
    -0.09095401714582904, 0.083353840546109, -0.0769325164113522,
    0.07143294629536133, -0.06666870588242046, 0.06250095514121304,
    -0.058823978658684585, 0.055555767627403614, -0.05263167937961666,
    0.05000004769810169, -0.047619070330142226, 0.04545455629320467,
    -0.04347826605304026, 0.04166666915034121, -0.04000000119214014,
    0.03846153903467518, -0.037037037312989324, 0.035714285847333355,
    -0.034482758684919304, 0.03333333336437758, -0.03225806453115042,
    0.03125000000727597, -0.030303030306558044, 0.029411764707594344,
    -0.02857142857226011, 0.027777777778181998, -0.027027027027223673,
    0.02631578947377995, -0.025641025641072283, 0.025000000000022737,
    -0.024390243902450117, 0.023809523809529224, -0.023255813953491015,
    0.02272727272727402, -0.022222222222222855, 0.021739130434782917,
    -0.021276595744681003, 0.02083333333333341, -0.02040816326530616,
    0.020000000000000018, -0.019607843137254912, 0.019230769230769235,
    -0.01886792452830189, 0.01851851851851852, -0.01818181818181818,
    0.017857142857142856, -0.017543859649122806, 0.017241379310344827,
    -0.01694915254237288 };

static double lgamma1p(double f)
{
    double p = LG1P[57];
    for (int i = 56; i >= 0; i--)
        p = p * f + LG1P[i];
    return f * (-EULER + f * p);
}

// Stirling's correction: lgamma(x) - ((x - 1/2) ln x - x + ln sqrt(2 pi)) = sum of B(2k) / (2k (2k - 1) x^(2k-1)),
// for x >= 12, where seven terms leave less than 2^-58 of it - 2^-62 of lgamma.
static double stirling_tail(double x)
{
    double z = 1.0 / (x * x);
    double p = 0.00641025641025641;                     // B14 / 182
    p = p * z - 0.0019175269175269176;                  // B12 / 132
    p = p * z + 0.0008417508417508417;                  // B10 / 90
    p = p * z - 0.0005952380952380953;                  // B8 / 56
    p = p * z + 0.0007936507936507937;                  // B6 / 30
    p = p * z - 0.002777777777777778;                   // B4 / 12
    p = p * z + 0.08333333333333333;                    // B2 / 2
    return p / x;
}

// lgamma(x) for x >= 12 by Stirling's series, as a pair. (x - 1/2) ln x and the x it loses are close in size, so the
// product is a pair too and the subtraction exact.
static void stirling_pair(double x, double* hi, double* lo)
{
    double lh, ll;
    __log_pair(x, &lh, &ll);
    double a = x - 0.5;                                  // exact
    double ph, pl;
    __two_product(a, lh, &ph, &pl);
    pl += a * ll;
    double sh, sl;
    __two_sum(ph, -x, &sh, &sl);
    double th, tl;
    __two_sum(sh, LN_SQRT_2PI, &th, &tl);
    tl += sl + pl + LN_SQRT_2PI_LO + stirling_tail(x);
    __fast_two_sum(th, tl, hi, lo);
}

// digamma(y) for y >= 2, to a few digits: what a change of y by its last bit does to lgamma(y).
static double digamma_rough(double y)
{
    return log(y) - 0.5 / y - 1.0 / (12.0 * y * y);
}

// sin(pi x), exactly 0 at the integers; |x| below 2^52.
static double sin_pi(double x)
{
    double n = rint(x);
    double r = x - n;                                    // exact, |r| <= 1/2
    double s = sin(PI_HI64 * r + PI_LO64 * r);
    if (fmod(n, 2.0) != 0.0)
        s = -s;
    return s;
}

// lgamma for x >= 1/2.
static double lgamma_positive(double x)
{
    if (x < 1.5)
        return lgamma1p(x - 1.0);
    if (x < 2.5)
        return log1p(x - 2.0) + lgamma1p(x - 2.0);
    if (x < 12.0)
    {
        // lgamma(x) = ln((x-1)(x-2)...(y)) + lgamma(y), y in [1.5, 2.5).
        double product = 1.0;
        double y = x;
        while (y >= 2.5)
        {
            y -= 1.0;
            product *= y;
        }
        return log(product) + log1p(y - 2.0) + lgamma1p(y - 2.0);
    }
    if (x > 0x1p60)
        return x * (log(x) - 1.0);
    double hi, lo;
    stirling_pair(x, &hi, &lo);
    return hi + lo;
}

double lgamma(double x)
{
    signgam = 1;
    if (x != x)
        return x + x;
    if (isinf(x))
        return HUGE_VAL;
    if (x >= 0.5)
        return lgamma_positive(x);
    if (x == 0.0 || (x < 0.0 && (trunc(x) == x)))
    {
        if (x == 0.0 && signbit(x))
            signgam = -1;
        errno = ERANGE;                                  // a pole
        return HUGE_VAL;
    }
    if (fabs(x) < 0x1p-60)
    {
        if (x < 0.0)
            signgam = -1;
        return -log(fabs(x));
    }
    // Gamma(x) Gamma(1 - x) = pi / sin(pi x), and Gamma(1 - x) > 0 here. 1 - x is a pair: rounding it would move
    // lgamma(1 - x) by its last bit times digamma, which grows with 1 - x.
    double s = sin_pi(x);
    if (s < 0.0)
        signgam = -1;
    double yh, yl;
    __two_sum(1.0, -x, &yh, &yl);
    double g = lgamma_positive(yh);
    if (yh >= 2.0)
        g += yl * digamma_rough(yh);
    return log(PI_HI64 / fabs(s)) - g;
}

// Gamma for x >= 1/2, up to where it overflows (the caller has checked).
static double tgamma_positive(double x)
{
    if (x < 12.0)
    {
        // Gamma(1 + f) = e^lgamma(1 + f) for f = x - n in [-1/2, 1/2], then up by Gamma(z + 1) = z Gamma(z).
        double n = rint(x);
        double f = x - n;                                // exact
        double g = exp(lgamma1p(f));                     // Gamma(1 + f)
        if (n == 0.0)
            return g / f;                                // x = 1/2, which rounds to 0: Gamma(1/2) = Gamma(3/2) / (1/2)
        // The product (1 + f)(2 + f)... as a pair, so ten factors round once, not ten times.
        double ph = 1.0, pl = 0.0;
        for (double k = 1.0; k < n; k += 1.0)
        {
            double ah, al, qh, ql;
            __two_sum(f, k, &ah, &al);
            __two_product(ph, ah, &qh, &ql);
            ql += ph * al + pl * ah;
            __fast_two_sum(qh, ql, &ph, &pl);
        }
        return g * ph + g * pl;
    }
    // e^lgamma(x), the exponent a pair: e^(hi + lo) = e^hi (1 + lo).
    double hi, lo;
    stirling_pair(x, &hi, &lo);
    return exp(hi) * (1.0 + lo);
}

double tgamma(double x)
{
    if (x != x)
        return x + x;
    if (isinf(x))
    {
        if (x > 0.0)
            return x;
        errno = EDOM;
        return NAN;
    }
    if (x == 0.0)
    {
        errno = ERANGE;                                  // a pole, on the side of the zero's sign
        return signbit(x) ? -HUGE_VAL : HUGE_VAL;
    }
    if (x < 0.0 && trunc(x) == x)
    {
        errno = EDOM;                                    // the negative integers
        return NAN;
    }
    if (x > 171.62434868396)
    {
        errno = ERANGE;
        return HUGE_VAL;
    }
    if (fabs(x) < 0x1p-54)
    {
        double r = 1.0 / x - EULER;
        if (isinf(r))
            errno = ERANGE;                              // x so small that 1/x overflows
        return r;
    }
    if (x >= 0.5)
        return tgamma_positive(x);
    if (x < -184.0)
    {
        errno = ERANGE;                                  // below the smallest double, with the sign it would have
        return fmod(floor(x), 2.0) == 0.0 ? 0.0 : -0.0;  // negative between -1 and 0, -3 and -2, ...
    }
    // Gamma(x) = pi / (sin(pi x) Gamma(1 - x)), 1 - x a pair as in lgamma.
    double s = sin_pi(x);
    double yh, yl;
    __two_sum(1.0, -x, &yh, &yl);
    double g = tgamma_positive(yh);
    if (isinf(g))
    {
        errno = ERANGE;
        return (s < 0.0) ? -0.0 : 0.0;
    }
    if (yh >= 2.0)
        g *= 1.0 + yl * digamma_rough(yh);
    double r = PI_HI64 / (s * g);
    if (r == 0.0)
        errno = ERANGE;
    return r;
}
