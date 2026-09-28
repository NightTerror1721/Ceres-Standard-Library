#include "math64_priv.h"

#include "math.h"
#include "errno.h"

// The rarer double functions, apart from src/math64.c so a program that only needs sin and sqrt does not carry them.

// ---- rounding ----------------------------------------------------------------------------------------------

// C's round(): halfway cases go AWAY from zero (rint, the `fround.d` instruction, takes them to even). Written as
// trunc plus a correction, not floor(x + 0.5), which is wrong for 0.49999999999999994.
double round(double x)
{
    double t = trunc(x);
    if (fabs(x - t) >= 0.5)
        t += copysign(1.0, x);
    return t;
}

static int int_from_rounded(double r)
{
    if (r >= 2147483648.0)
    {
        errno = ERANGE;
        return 2147483647;
    }
    if (r < -2147483648.0)
    {
        errno = ERANGE;
        return -2147483647 - 1;
    }
    return (int)r;
}

// -2^63 is a double, and the only one at the bottom edge; 2^63 is the first one past the top.
static long long ll_from_rounded(double r)
{
    if (r >= 9223372036854775808.0)
    {
        errno = ERANGE;
        return 9223372036854775807LL;
    }
    if (r < -9223372036854775808.0)
    {
        errno = ERANGE;
        return -9223372036854775807LL - 1;
    }
    return (long long)r;
}

int lround(double x)
{
    if (x != x)
    {
        errno = EDOM;
        return 0;
    }
    return int_from_rounded(round(x));
}

int lrint(double x)
{
    if (x != x)
    {
        errno = EDOM;
        return 0;
    }
    return int_from_rounded(rint(x));
}

long long llround(double x)
{
    if (x != x)
    {
        errno = EDOM;
        return 0;
    }
    return ll_from_rounded(round(x));
}

long long llrint(double x)
{
    if (x != x)
    {
        errno = EDOM;
        return 0;
    }
    return ll_from_rounded(rint(x));
}

// ---- the exponent ------------------------------------------------------------------------------------------

double ldexp(double x, int e)
{
    if (x == 0.0 || isinf(x) || x != x)
        return x;
    double r = __scalbn64(x, e);
    if (isinf(r) || r == 0.0)
        errno = ERANGE;
    return r;
}

double frexp(double x, int* e)
{
    *e = 0;
    if (x == 0.0 || isinf(x) || x != x)
        return x;
    __u64 u = __dbits(x);
    int adjust = 0;
    if (((u >> 52) & 0x7FFu) == 0)                      // subnormal: scale it into the normal range first
    {
        u = __dbits(x * 0x1p54);
        adjust = -54;
    }
    *e = (int)((u >> 52) & 0x7FFu) - 1022 + adjust;
    return __dfrom((u & 0x800FFFFFFFFFFFFFull) | 0x3FE0000000000000ull);   // exponent field 1022: [0.5, 1)
}

double modf(double x, double* ip)
{
    if (isinf(x))
    {
        *ip = x;
        return copysign(0.0, x);
    }
    if (x != x)
    {
        *ip = x;
        return x;
    }
    double whole = trunc(x);
    *ip = whole;
    return copysign(x - whole, x);                      // a whole x has a fraction of exactly +-0
}

int ilogb(double x)
{
    if (x == 0.0 || x != x)
    {
        errno = EDOM;
        return FP_ILOGB0;
    }
    if (isinf(x))
    {
        errno = EDOM;
        return 2147483647;
    }
    int e;
    frexp(x, &e);
    return e - 1;
}

double logb(double x)
{
    if (x != x)
        return x;
    if (isinf(x))
        return HUGE_VAL;
    if (x == 0.0)
    {
        errno = ERANGE;                                 // a pole
        return -HUGE_VAL;
    }
    return (double)ilogb(x);
}

double nextafter(double x, double y)
{
    if (x != x || y != y)
        return x + y;
    if (x == y)
        return y;
    __u64 u;
    if (x == 0.0)
        u = signbit(y) ? 0x8000000000000001ull : 1ull;  // the smallest subnormal, towards y
    else
    {
        u = __dbits(x);
        if ((x < y) == (x > 0.0))
            u++;                                        // away from zero
        else
            u--;
    }
    double r = __dfrom(u);
    if (isinf(r) || fabs(r) < 0x1p-1022)
        errno = ERANGE;                                 // overflowed, or subnormal or zero
    return r;
}

double nan(const char* tag) { (void)tag; return NAN; }

// ---- simple arithmetic -------------------------------------------------------------------------------------

double fdim(double x, double y)
{
    if (x != x || y != y)
        return x + y;
    return x > y ? x - y : 0.0;
}

double remainder(double x, double y)
{
    if (x != x || y != y || isinf(x) || y == 0.0)
    {
        if (x == x && y == y)
            errno = EDOM;
        return NAN;
    }
    if (isinf(y))
        return x;
    double ay = fabs(y);
    double r = fmod(fabs(x), ay);                       // 0 <= r < |y|
    if (r == 0.0)
        return copysign(0.0, x);                        // an exact multiple: zero with the sign of x
    // Nearer the next multiple, or halfway with an odd quotient: r - |y|. r > |y|/2 is asked without adding r
    // to itself, which could overflow.
    double half = 0.5 * ay;
    if (r > half || (r == half && fmod(fabs(x), 2.0 * ay) >= ay))
        r -= ay;
    return x < 0.0 ? -r : r;                            // r was found for |x|: mirror it for a negative x
}

double hypot(double x, double y)
{
    double a = fabs(x);
    double b = fabs(y);
    if (isinf(a) || isinf(b))
        return HUGE_VAL;                                // even with a NaN beside it
    if (a != a || b != b)
        return a + b;
    if (a < b)
    {
        double t = a;
        a = b;
        b = t;
    }
    if (b == 0.0)
        return a;
    if (a > b * 0x1p60)
        return a + b;
    double scale = 1.0;
    if (a > 0x1p500)
    {
        a *= 0x1p-600;
        b *= 0x1p-600;
        scale = 0x1p600;
    }
    else if (b < 0x1p-500)
    {
        a *= 0x1p600;
        b *= 0x1p600;
        scale = 0x1p-600;
    }
    // a^2 + b^2 as a pair, so the square root rounds once.
    double ah, al, bh, bl, sh, sl;
    __two_product(a, a, &ah, &al);
    __two_product(b, b, &bh, &bl);
    __two_sum(ah, bh, &sh, &sl);
    sl += al + bl;
    double s = sh + sl;
    double r = sqrt(s);
    // One Newton step on the pair: r + (s - r^2) / (2r).
    double rh, rl;
    __two_product(r, r, &rh, &rl);
    r += ((sh - rh) + (sl - rl)) / (2.0 * r);
    r *= scale;
    if (isinf(r))
        errno = ERANGE;
    return r;
}

double cbrt(double x)
{
    if (x == 0.0 || x != x || isinf(x))
        return x + x;
    double ax = fabs(x);
    int k = 0;
    if (ax < 0x1p-1000)
    {
        ax *= 0x1p162;                                  // 2^(3*54): the root scales by exactly 2^54
        k = -54;
    }
    // A first guess from the exponent divided by 3 (fdlibm's, good to about 5 bits), then Newton's steps.
    double t = __dfrom((__u64)(unsigned int)((unsigned int)__dhigh(ax) / 3u + 715094163u) << 32);
    for (int i = 0; i < 4; i++)
        t = t - (t * t * t - ax) / (3.0 * t * t);
    // A last step on the residual as a pair: t + (ax - t^3) / (3 t^2).
    double sh, sl;
    __two_product(t, t, &sh, &sl);
    double ch, cl;
    __two_product(sh, t, &ch, &cl);
    cl += sl * t;
    t += ((ax - ch) - cl) / (3.0 * t * t);
    if (k != 0)
        t = __scalbn64(t, k);
    return x < 0.0 ? -t : t;
}

// ---- e^x - 1, log(1 + x) -----------------------------------------------------------------------------------

// e^r - 1 for |r| < 1 by its Taylor series, r(1 + r/2 (1 + r/3 (1 + ...))): each factor shrinks the error of the
// ones inside it, so the sum is right to about an ulp. Twenty-two terms reach 1/23!, below 2^-74.
static double expm1_taylor(double r)
{
    double t = 1.0;
    for (int n = 22; n >= 2; n--)
        t = 1.0 + r * t / n;
    return r * t;
}

// e^x - 1 = 2^k (e^r - 1) + (2^k - 1) with x = k ln2 + r, r a pair: 2^k - 1 is exact, and the sum rounds once.
double expm1(double x)
{
    if (x != x)
        return x + x;
    if (x > 709.782712893383973096)
    {
        if (!isinf(x))
            errno = ERANGE;
        return HUGE_VAL;
    }
    if (x < -40.0)
        return -1.0;                                    // e^x below 2^-57
    double ax = fabs(x);
    if (ax < 0x1p-54)
        return x;
    if (ax < 1.0)
        return expm1_taylor(x);                         // below 1 the reduction below would cancel a bit or two
    if (x > 700.0)
        return exp(x);                                  // the -1 is far below the last bit
    double k = rint(x * INV_LN2_HI64);
    double hi = x - k * 6.93147180369123816490e-01;    // fdlibm's ln2 in two parts: k * the first is exact
    double lo = k * 1.90821492927058770002e-10;
    double r = hi - lo;
    double rl = (hi - r) - lo;
    double e = expm1_taylor(r);
    e += rl * (1.0 + e);                                // e^(r + rl) - 1 = (e^r - 1) + rl e^r
    double two_k = __scalbn64(1.0, (int)k);
    return two_k * e + (two_k - 1.0);
}

// ln(1 + x) with 1 + x as a pair: ln(uh + ul) = ln(uh) + ul/uh, ln(uh) a pair too.
double log1p(double x)
{
    if (x != x)
        return x + x;
    if (x < -1.0)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == -1.0)
    {
        errno = ERANGE;
        return -HUGE_VAL;
    }
    if (isinf(x))
        return x;
    if (fabs(x) < 0x1p-54)
        return x;
    double uh, ul;
    __two_sum(1.0, x, &uh, &ul);
    double lh, ll;
    __log_pair(uh, &lh, &ll);
    return lh + (ll + ul / uh);
}

// ---- hyperbolic functions ----------------------------------------------------------------------------------

// e^|x| / 2 for the large arguments, without overflowing on the way there.
static double half_exp(double ax)
{
    if (ax < 709.782712893383973096)
        return 0.5 * exp(ax);
    if (ax <= 710.4758600739439)                       // e^x/2 still fits
    {
        double w = exp(0.5 * ax);
        return (0.5 * w) * w;
    }
    errno = ERANGE;
    return HUGE_VAL;
}

// sinh and cosh for |x| < 1 by their Taylor series in y = x^2, summed from the smallest term up: x(1 + y/(2*3)(1 +
// y/(4*5)(1 + ...))) and 1 + y/(1*2)(1 + y/(3*4)(1 + ...)). Twelve terms reach 1/25!, below 2^-83.
static double sinh_taylor(double x)
{
    double y = x * x;
    double t = 1.0;
    for (int n = 12; n >= 1; n--)
        t = 1.0 + y * t / ((2.0 * n) * (2.0 * n + 1.0));
    return x * t;
}

static double cosh_taylor(double x)
{
    double y = x * x;
    double t = 1.0;
    for (int n = 12; n >= 1; n--)
        t = 1.0 + y * t / ((2.0 * n - 1.0) * (2.0 * n));
    return t;
}

double sinh(double x)
{
    if (x != x || isinf(x))
        return x + x;
    double ax = fabs(x);
    double h = x < 0.0 ? -0.5 : 0.5;
    if (ax < 22.0)
    {
        if (ax < 0x1p-28)
            return x;
        if (ax < 1.0)
            return sinh_taylor(x);
        double t = expm1(ax);
        return h * (t + t / (t + 1.0));
    }
    double r = half_exp(ax);
    return x < 0.0 ? -r : r;
}

double cosh(double x)
{
    if (x != x)
        return x + x;
    double ax = fabs(x);
    if (isinf(ax))
        return ax;
    if (ax < 0.5 * LN2_HI64)
    {
        double t = expm1(ax);
        double w = 1.0 + t;
        return 1.0 + (t * t) / (w + w);
    }
    if (ax < 22.0)
    {
        double t = exp(ax);
        return 0.5 * t + 0.5 / t;
    }
    return half_exp(ax);
}

double tanh(double x)
{
    if (x != x)
        return x + x;
    double ax = fabs(x);
    double z;
    if (ax < 22.0)
    {
        if (ax < 0x1p-55)
            return x;
        if (ax >= 1.0)
        {
            double t = expm1(2.0 * ax);
            z = 1.0 - 2.0 / (t + 2.0);
        }
        else
            z = sinh_taylor(ax) / cosh_taylor(ax);
    }
    else
        z = 1.0;                                        // (and for an infinity)
    return x < 0.0 ? -z : z;
}

double asinh(double x)
{
    if (x != x || isinf(x))
        return x + x;
    double ax = fabs(x);
    double w;
    if (ax < 0x1p-28)
        return x;
    if (ax > 0x1p28)
        w = log(ax) + LN2_HI64;
    else if (ax > 2.0)
        w = log(2.0 * ax + 1.0 / (sqrt(ax * ax + 1.0) + ax));
    else
    {
        double t = ax * ax;
        w = log1p(ax + t / (1.0 + sqrt(1.0 + t)));
    }
    return x < 0.0 ? -w : w;
}

double acosh(double x)
{
    if (x != x)
        return x + x;
    if (x < 1.0)
    {
        errno = EDOM;
        return NAN;
    }
    if (isinf(x))
        return x;
    if (x > 0x1p28)
        return log(x) + LN2_HI64;
    if (x == 1.0)
        return 0.0;
    if (x > 2.0)
        return log(2.0 * x - 1.0 / (x + sqrt(x * x - 1.0)));
    double t = x - 1.0;
    return log1p(t + sqrt(2.0 * t + t * t));
}

double atanh(double x)
{
    if (x != x)
        return x + x;
    double ax = fabs(x);
    if (ax > 1.0)
    {
        errno = EDOM;
        return NAN;
    }
    if (ax == 1.0)
    {
        errno = ERANGE;
        return x > 0.0 ? HUGE_VAL : -HUGE_VAL;
    }
    if (ax < 0x1p-28)
        return x;
    double t;
    if (ax < 0.5)
    {
        t = ax + ax;
        t = 0.5 * log1p(t + t * ax / (1.0 - ax));
    }
    else
        t = 0.5 * log1p((ax + ax) / (1.0 - ax));
    return x < 0.0 ? -t : t;
}
