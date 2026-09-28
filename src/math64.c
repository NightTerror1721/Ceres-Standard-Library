#include "math64_priv.h"

#include "math.h"
#include "errno.h"

// The double functions: exponentials, logarithms, pow, and the trigonometric functions with their inverses. The
// single-instruction ones (fabs, sqrt, floor, ceil, trunc, rint, fmin, fmax, copysign, fma) are in
// asm/math_ops.casm, fmod in src/fmod.c; the rarer functions are in math64_ext.c and math64_special.c.
//
// The kernels are fdlibm's (Sun Microsystems, 1993: "Permission to use, copy, modify, and distribute this software
// is freely granted, provided that this notice is preserved"), rewritten for this library; the logarithms, pow and
// exp2 are computed in double-double (math64_priv.h) instead, and the large-argument reduction is written here.
//
// A double division by zero does not fail: it sets the Trap flag and leaves the destination unchanged. So every
// division below has a divisor that cannot be zero.

// ---- scaling by a power of two -----------------------------------------------------------------------------

double __scalbn64(double x, int n)
{
    double y = x;
    if (n > 1023)
    {
        y *= 0x1p1023;
        n -= 1023;
        if (n > 1023)
        {
            y *= 0x1p1023;
            n -= 1023;
            if (n > 1023)
                n = 1023;
        }
    }
    else if (n < -1022)
    {
        // 2^-1022 * 2^53: the first product stays normal, so the one rounding is the last multiplication's.
        y *= 0x1p-969;
        n += 1022 - 53;
        if (n < -1022)
        {
            y *= 0x1p-969;
            n += 1022 - 53;
            if (n < -1022)
                n = -1022;
        }
    }
    return y * __dfrom((__u64)(0x3FF + n) << 52);
}

// ---- e^x ----------------------------------------------------------------------------------------------------

// 1/3! .. 1/15!: for |z| <= ln2/2 the terms after z^15/15! are below 2^-63.
static const double EXP_TAIL[13] = {
    0.16666666666666666, 0.041666666666666664, 0.008333333333333333, 0.001388888888888889,
    0.0001984126984126984, 2.48015873015873e-05, 2.7557319223985893e-06, 2.755731922398589e-07,
    2.505210838544172e-08, 2.08767569878681e-09, 1.6059043836821613e-10, 1.1470745597729725e-11,
    7.647163731819816e-13 };

// e^z = 1 + z + z^2/2 + z^3 (1/3! + z/4! + ...), the first three terms as exact pairs and the rest, below 0.007,
// in plain double: the sum is good to about 2^-60 before its one rounding, so the result is the nearest double
// unless the exact value lies within a few hundredths of an ulp of a halfway point.
double __exp_kernel64(double hi, double lo, int k)
{
    double zh, zl;
    __two_sum(hi, -lo, &zh, &zl);
    double p = EXP_TAIL[12];
    for (int i = 11; i >= 0; i--)
        p = p * zh + EXP_TAIL[i];
    double tail = zh * zh * zh * p;
    double ah, al;
    __fast_two_sum(1.0, zh, &ah, &al);
    double bh, bl;
    __two_product(zh, zh, &bh, &bl);
    double ch, cl;
    __fast_two_sum(ah, 0.5 * bh, &ch, &cl);
    // zl moves the result by zl e^zh, e^zh to its first three terms.
    cl += al + 0.5 * bl + tail + zl * (1.0 + zh * (1.0 + 0.5 * zh));
    double y = ch + cl;
    return k == 0 ? y : __scalbn64(y, k);
}

// ln 2 in two parts: k * LN2_EXP_HI is exact for every k exp needs.
#define LN2_EXP_HI 6.93147180369123816490e-01
#define LN2_EXP_LO 1.90821492927058770002e-10

double exp(double x)
{
    if (x != x)
        return x + x;
    if (x > 709.782712893383973096)
    {
        if (!isinf(x))
            errno = ERANGE;                              // exp(+inf) is +inf without a range error
        return HUGE_VAL;
    }
    if (x < -745.13321910194110842)
    {
        if (!isinf(x))
            errno = ERANGE;
        return 0.0;
    }
    double ax = fabs(x);
    if (ax < 0x1p-54)
        return 1.0 + x;
    if (ax <= 0.5 * LN2_HI64)
        return __exp_kernel64(x, 0.0, 0);
    int k = (int)(x * INV_LN2_HI64 + (x < 0.0 ? -0.5 : 0.5));
    double hi = x - k * LN2_EXP_HI;
    double lo = k * LN2_EXP_LO;
    return __exp_kernel64(hi, lo, k);
}

double __exp2_pair(double hi, double lo)
{
    double n = rint(hi);
    double r = hi - n;                                   // exact: |r| <= 1/2
    // r + lo, times ln 2, as a pair: that is the natural exponent the kernel takes.
    double zh, zl;
    __two_product(r, LN2_HI64, &zh, &zl);
    zl += r * LN2_LO64 + lo * LN2_HI64;
    __fast_two_sum(zh, zl, &zh, &zl);
    return __exp_kernel64(zh, -zl, (int)n);
}

double exp2(double x)
{
    if (x != x)
        return x + x;
    if (x >= 1024.0)
    {
        if (!isinf(x))
            errno = ERANGE;
        return HUGE_VAL;
    }
    if (x < -1075.0)
    {
        if (!isinf(x))
            errno = ERANGE;
        return 0.0;
    }
    double r = __exp2_pair(x, 0.0);
    if (r == 0.0)
        errno = ERANGE;
    return r;
}

// ---- logarithms ---------------------------------------------------------------------------------------------

// 2/(2n+1) for n = 1..12: log(m) = 2 atanh(s) = 2s + s * (2/3 s^2 + 2/5 s^4 + ...), s = (m - 1)/(m + 1) <= 0.1716.
// (The first, 2/3, is used as the pair below.)
#define TWO_THIRDS_HI 0.6666666666666666
#define TWO_THIRDS_LO 3.700743415417188e-17
static const double ATANH_TERMS[12] = {
    0.6666666666666666, 0.4, 0.2857142857142857, 0.2222222222222222, 0.18181818181818182, 0.15384615384615385, 0.13333333333333333, 0.11764705882352941, 0.10526315789473684, 0.09523809523809523, 0.08695652173913043, 0.08 };

void __log2_pair(double x, double* hi, double* lo)
{
    // x = 2^k * m with m in [1/sqrt2, sqrt2).
    int k = 0;
    __u64 b = __dbits(x);
    if ((b >> 52) == 0)                                  // subnormal
    {
        x *= 0x1p54;
        b = __dbits(x);
        k = -54;
    }
    k += (int)(b >> 52) - 1023;
    double m = __dfrom((b & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull);
    if (m > 1.4142135623730951)
    {
        m *= 0.5;
        k++;
    }
    double f = m - 1.0;                                  // exact

    // s = f / (2 + f) as a pair: the division's remainder, worked out exactly, gives the part it rounded away.
    double dh, dl;
    __fast_two_sum(2.0, f, &dh, &dl);
    double sh = f / dh;
    double ph, pl;
    __two_product(sh, dh, &ph, &pl);
    double sl = (((f - ph) - pl) - sh * dl) / dh;

    // ln m = 2s + s A, A = (2/3) z + z^2 (2/5 + 2/7 z + ...), z = s^2. The first term of A, below 2% of 1, is
    // kept as a pair; the rest, below 0.04%, rounds far past the last bit that matters.
    double zh, zl;
    __two_product(sh, sh, &zh, &zl);
    zl += 2.0 * sh * sl;
    double q = ATANH_TERMS[11];
    for (int i = 10; i >= 1; i--)
        q = q * zh + ATANH_TERMS[i];
    double ch, cl;
    __two_product(zh, TWO_THIRDS_HI, &ch, &cl);
    cl += zh * TWO_THIRDS_LO + zl * TWO_THIRDS_HI;
    double ah, al;
    __two_sum(ch, zh * zh * q, &ah, &al);
    al += cl;
    // s A = sh ah + (sh al + sl ah), and 2s + s A with its three parts summed exactly.
    double eh, el;
    __two_product(sh, ah, &eh, &el);
    double lh, ll;
    __two_sum(2.0 * sh, eh, &lh, &ll);
    ll += el + 2.0 * sl + sh * al + sl * ah;
    __fast_two_sum(lh, ll, &lh, &ll);

    // log2 m = ln m / ln 2, then k added exactly.
    double qh, ql;
    __two_product(lh, INV_LN2_HI64, &qh, &ql);
    ql += lh * INV_LN2_LO64 + ll * INV_LN2_HI64;
    double th, tl;
    __two_sum((double)k, qh, &th, &tl);
    tl += ql;
    __fast_two_sum(th, tl, hi, lo);
}

// NaN, a negative x, zero and +inf: 1 and the answer in *out, or 0 for an ordinary positive x.
static int log_special(double x, double* out)
{
    if (x != x) { *out = x + x; return 1; }
    if (x < 0.0) { errno = EDOM; *out = NAN; return 1; }
    if (x == 0.0) { errno = ERANGE; *out = -HUGE_VAL; return 1; }
    if (isinf(x)) { *out = x; return 1; }
    return 0;
}

double log2(double x)
{
    double r;
    if (log_special(x, &r))
        return r;
    double hi, lo;
    __log2_pair(x, &hi, &lo);
    return hi + lo;
}

// log2 x times a constant given as a pair, as a pair: ln x and log10 x round once from it.
static void log2_times(double x, double ch, double cl, double* hi, double* lo)
{
    double h, l;
    __log2_pair(x, &h, &l);
    double ph, pl;
    __two_product(h, ch, &ph, &pl);
    pl += h * cl + l * ch;
    __fast_two_sum(ph, pl, hi, lo);
}

void __log_pair(double x, double* hi, double* lo)
{
    log2_times(x, LN2_HI64, LN2_LO64, hi, lo);
}

double log(double x)
{
    double r;
    if (log_special(x, &r))
        return r;
    double hi, lo;
    log2_times(x, LN2_HI64, LN2_LO64, &hi, &lo);
    return hi + lo;
}

double log10(double x)
{
    double r;
    if (log_special(x, &r))
        return r;
    double hi, lo;
    log2_times(x, LOG10_2_HI64, LOG10_2_LO64, &hi, &lo);
    return hi + lo;
}

// ---- pow ------------------------------------------------------------------------------------------------

// 1 when y is an odd integer.
static int is_odd_integer(double y)
{
    if (trunc(y) != y || fabs(y) >= 0x1p53)
        return 0;                                        // past 2^53 every double is even
    return (int)((long long)y & 1);
}

double pow(double x, double y)
{
    if (y == 0.0 || x == 1.0)
        return 1.0;                                      // even when the other one is NaN
    if (x != x || y != y)
        return x + y;

    int odd = is_odd_integer(y);
    if (isinf(y))
    {
        double ax = fabs(x);
        if (ax == 1.0)
            return 1.0;
        return ((ax > 1.0) == (y > 0.0)) ? HUGE_VAL : 0.0;
    }
    if (isinf(x))
    {
        if (y > 0.0)
            return (x < 0.0 && odd) ? -HUGE_VAL : HUGE_VAL;
        return (x < 0.0 && odd) ? -0.0 : 0.0;
    }
    if (x == 0.0)
    {
        int negative_zero = signbit(x);
        if (y > 0.0)
            return (negative_zero && odd) ? -0.0 : 0.0;
        errno = ERANGE;                                  // 0 to a negative power: a pole
        return (negative_zero && odd) ? -HUGE_VAL : HUGE_VAL;
    }

    int negative = 0;
    if (x < 0.0)
    {
        if (trunc(y) != y)
        {
            errno = EDOM;                                // a negative base needs a whole exponent
            return NAN;
        }
        negative = odd;
        x = -x;
    }

    // x^y = 2^(y log2 x), the exponent as a pair: its error, about 2^-100 relative, is what the result carries.
    double lh, ll;
    __log2_pair(x, &lh, &ll);
    double t = y * lh;
    double r;
    if (t > 1100.0)
        r = HUGE_VAL;
    else if (t < -1100.0)
        r = 0.0;
    else
    {
        double th, tl;
        __two_product(y, lh, &th, &tl);
        tl += y * ll;
        __fast_two_sum(th, tl, &th, &tl);
        if (th >= 1024.0)
            r = HUGE_VAL;
        else if (th < -1076.0)
            r = 0.0;
        else
            r = __exp2_pair(th, tl);
    }
    if (isinf(r) || r == 0.0)
        errno = ERANGE;
    return negative ? -r : r;
}

// ---- argument reduction for the trigonometric functions -----------------------------------------------------

// 2/pi, the first 1664 bits after the binary point: enough for any double, the largest being below 2^1024.
static const unsigned int TWO_OVER_PI[52] = {
    0xA2F9836Eu, 0x4E441529u, 0xFC2757D1u, 0xF534DDC0u,
    0xDB629599u, 0x3C439041u, 0xFE5163ABu, 0xDEBBC561u,
    0xB7246E3Au, 0x424DD2E0u, 0x06492EEAu, 0x09D1921Cu,
    0xFE1DEB1Cu, 0xB129A73Eu, 0xE88235F5u, 0x2EBB4484u,
    0xE99C7026u, 0xB45F7E41u, 0x3991D639u, 0x835339F4u,
    0x9C845F8Bu, 0xBDF9283Bu, 0x1FF897FFu, 0xDE05980Fu,
    0xEF2F118Bu, 0x5A0A6D1Fu, 0x6D367ECFu, 0x27CB09B7u,
    0x4F463F66u, 0x9E5FEA2Du, 0x7527BAC7u, 0xEBE5F17Bu,
    0x3D0739F7u, 0x8A5292EAu, 0x6BFB5FB1u, 0x1F8D5D08u,
    0x56033046u, 0xFC7B6BABu, 0xF0CFBC20u, 0x9AF4361Du,
    0xA9E39161u, 0x5EE61B08u, 0x6599855Fu, 0x14A06840u,
    0x8DFFD880u, 0x4D732731u, 0x06061556u, 0xCA73A8C9u,
    0x60E27BC0u, 0x8C6B47C4u, 0x19C36806u, 0x08195100u };

// 32 bits of 2/pi from bit `b` after the point (bit 0 is worth 1/2); the bits before the point are 0.
static unsigned int two_over_pi_window(int b)
{
    int w = b >= 0 ? b / 32 : -((31 - b) / 32);
    int s = b - w * 32;
    unsigned int a = w >= 0 && w < 52 ? TWO_OVER_PI[w] : 0u;
    unsigned int c = w + 1 >= 0 && w + 1 < 52 ? TWO_OVER_PI[w + 1] : 0u;
    return s == 0 ? a : (a << s) | (c >> (32 - s));
}

// Payne and Hanek's reduction of a positive finite x: x * 2/pi from integers. x is m * 2^e with m the 53-bit
// significand; the bits of 2/pi that m * 2^e would carry past 4 cannot change the quadrant and are skipped, and a
// 192-bit window of the rest gives the product's fraction to more than 130 bits - the worst double, a hair from a
// multiple of pi/2, cancels about 61 of them.
static int reduce_large(double ax, double* y0, double* y1)
{
    __u64 b = __dbits(ax);
    __u64 m = (b & 0x000FFFFFFFFFFFFFull) | 0x0010000000000000ull;
    int e = (int)(b >> 52) - 1075;
    int from = e - 2;
    // The window V as six 32-bit limbs, most significant first; m as two.
    unsigned int v[6];
    for (int i = 0; i < 6; i++)
        v[i] = two_over_pi_window(from + 32 * i);
    unsigned int mh = (unsigned int)(m >> 32);
    unsigned int ml = (unsigned int)m;
    // P = m * V, eight limbs, least significant first. The binary point is 190 bits up.
    unsigned int p[8];
    for (int i = 0; i < 8; i++)
        p[i] = 0;
    for (int i = 0; i < 6; i++)
    {
        unsigned int vi = v[5 - i];                      // limb i of V, from the bottom
        __u64 carry = 0;
        for (int j = 0; j < 2; j++)
        {
            __u64 t = (__u64)(j == 0 ? ml : mh) * vi + p[i + j] + carry;
            p[i + j] = (unsigned int)t;
            carry = t >> 32;
        }
        for (int k = i + 2; k < 8 && carry != 0; k++)
        {
            __u64 t = (__u64)p[k] + carry;
            p[k] = (unsigned int)t;
            carry = t >> 32;
        }
    }
    // Bits 190 and 191 are the quadrant; bits 189 down to 62 the first 128 of the fraction.
    int q = (int)((p[5] >> 30) & 3u);
    __u64 fh = ((__u64)(p[5] & 0x3FFFFFFFu) << 34) | ((__u64)p[4] << 2) | (p[3] >> 30);
    __u64 fl = ((__u64)(p[3] & 0x3FFFFFFFu) << 34) | ((__u64)p[2] << 2) | (p[1] >> 30);
    // To the nearest quadrant: a fraction of a half or more is the next one, from below.
    int negative = 0;
    if (fh >> 63)
    {
        q++;
        negative = 1;
        fl = ~fl + 1u;                                   // 2^128 - F, the distance up to the next quadrant
        fh = ~fh + (fl == 0 ? 1u : 0u);
    }
    if (fh == 0 && fl == 0)
    {
        *y0 = 0.0;
        *y1 = 0.0;
        return q;
    }
    int lz = 0;
    while ((fh >> 63) == 0)
    {
        fh = (fh << 1) | (fl >> 63);
        fl <<= 1;
        lz++;
    }
    // F / 2^128 as a pair: its top 53 bits and the next 53, each scaled into place.
    double d1 = (double)(fh >> 11) * __dfrom((__u64)(1023 - 53 - lz) << 52);
    double d2 = (double)(((fh & 0x7FFu) << 42) | (fl >> 22)) * __dfrom((__u64)(1023 - 106 - lz) << 52);
    // times pi/2.
    double rh, rl;
    __two_product(d1, PIO2_HI64, &rh, &rl);
    rl += d1 * PIO2_LO64 + d2 * PIO2_HI64;
    __fast_two_sum(rh, rl, y0, y1);
    if (negative)
    {
        *y0 = -*y0;
        *y1 = -*y1;
    }
    return q;
}

// pi/2 in three parts of 33 bits, each with the double of what is left after it (fdlibm's rem_pio2).
#define PIO2_1  1.57079632673412561417e+00
#define PIO2_1T 6.07710050650619224932e-11
#define PIO2_2  6.07710050630396597660e-11
#define PIO2_2T 2.02226624879595063154e-21
#define PIO2_3  2.02226624871116645580e-21
#define PIO2_3T 8.47842766036889956997e-32

int __rem_pio2_64(double x, double* y0, double* y1)
{
    double ax = fabs(x);
    if (ax <= 0.7853981633974483)
    {
        *y0 = x;
        *y1 = 0.0;
        return 0;
    }
    int n;
    if (ax <= 823549.6)                                  // 2^19 * pi/2: n * PIO2_1 is exact
    {
        n = (int)(ax * 0.6366197723675814 + 0.5);
        double fn = (double)n;
        double r = ax - fn * PIO2_1;
        double w = fn * PIO2_1T;
        double y = r - w;
        int j = __dhigh(ax) >> 20;
        int i = j - ((__dhigh(y) >> 20) & 0x7FF);
        if (i > 16)                                      // cancellation: a second step, good to 118 bits
        {
            double t = r;
            w = fn * PIO2_2;
            r = t - w;
            w = fn * PIO2_2T - ((t - r) - w);
            y = r - w;
            i = j - ((__dhigh(y) >> 20) & 0x7FF);
            if (i > 49)                                  // and a third, good to 151
            {
                t = r;
                w = fn * PIO2_3;
                r = t - w;
                w = fn * PIO2_3T - ((t - r) - w);
                y = r - w;
            }
        }
        *y0 = y;
        *y1 = (r - y) - w;
    }
    else
        n = reduce_large(ax, y0, y1);
    if (x < 0.0)
    {
        *y0 = -*y0;
        *y1 = -*y1;
        return -n;
    }
    return n;
}

// ---- sin, cos, tan ------------------------------------------------------------------------------------

static const double S1 = -1.66666666666666324348e-01;
static const double S2 = 8.33333333332248946124e-03;
static const double S3 = -1.98412698298579493134e-04;
static const double S4 = 2.75573137070700676789e-06;
static const double S5 = -2.50507602534068634195e-08;
static const double S6 = 1.58969099521155010221e-10;

double __sin_kernel64(double x, double y)
{
    if (fabs(x) < 0x1p-27)
        return x;
    double z = x * x;
    double v = z * x;
    double r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    if (y == 0.0)
        return x + v * (S1 + z * r);
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

static const double C1 = 4.16666666666666019037e-02;
static const double C2 = -1.38888888888741095749e-03;
static const double C3 = 2.48015872894767294178e-05;
static const double C4 = -2.75573143513906633035e-07;
static const double C5 = 2.08757232129817482790e-09;
static const double C6 = -1.13596475577881948265e-11;

double __cos_kernel64(double x, double y)
{
    double ax = fabs(x);
    if (ax < 0x1p-27)
        return 1.0;
    double z = x * x;
    double r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
    if (ax < 0.3)
        return 1.0 - (0.5 * z - (z * r - x * y));
    // 1 - z/2 as (1 - qx) - (z/2 - qx), qx about x^2/4, so the subtraction from 1 is exact.
    double qx = ax > 0.78125 ? 0.28125 : __dfrom((__u64)(unsigned int)(__dhigh(ax) - 0x00200000) << 32);
    double hz = 0.5 * z - qx;
    double a = 1.0 - qx;
    return a - (hz - (z * r - x * y));
}

// NaN in, NaN out; an infinite argument has no sine: EDOM.
static int trig_domain(double x, double* out)
{
    if (x != x) { *out = x + x; return 1; }
    if (isinf(x)) { errno = EDOM; *out = NAN; return 1; }
    return 0;
}

double sin(double x)
{
    double r;
    if (trig_domain(x, &r))
        return r;
    double y0, y1;
    switch (__rem_pio2_64(x, &y0, &y1) & 3)
    {
        case 0: return __sin_kernel64(y0, y1);
        case 1: return __cos_kernel64(y0, y1);
        case 2: return -__sin_kernel64(y0, y1);
        default: return -__cos_kernel64(y0, y1);
    }
}

double cos(double x)
{
    double r;
    if (trig_domain(x, &r))
        return r;
    double y0, y1;
    switch (__rem_pio2_64(x, &y0, &y1) & 3)
    {
        case 0: return __cos_kernel64(y0, y1);
        case 1: return -__sin_kernel64(y0, y1);
        case 2: return -__cos_kernel64(y0, y1);
        default: return __sin_kernel64(y0, y1);
    }
}

void sincos(double x, double* s, double* c)
{
    double r;
    if (trig_domain(x, &r))
    {
        *s = r;
        *c = r;
        return;
    }
    double y0, y1;
    int n = __rem_pio2_64(x, &y0, &y1) & 3;
    double sn = __sin_kernel64(y0, y1);
    double cs = __cos_kernel64(y0, y1);
    switch (n)
    {
        case 0: *s = sn; *c = cs; break;
        case 1: *s = cs; *c = -sn; break;
        case 2: *s = -sn; *c = -cs; break;
        default: *s = -cs; *c = sn; break;
    }
}

// sin/cos of the reduced argument: each within an ulp, so the quotient within about two and a half. The divisor is
// the larger of the two, and never zero.
double tan(double x)
{
    double r;
    if (trig_domain(x, &r))
        return r;
    double y0, y1;
    int n = __rem_pio2_64(x, &y0, &y1);
    if (y0 == 0.0)
        return (n & 1) ? -HUGE_VAL : y0;                 // only for x = +-0: n * pi/2 is never a double otherwise
    double sn = __sin_kernel64(y0, y1);
    double cs = __cos_kernel64(y0, y1);
    return (n & 1) ? -cs / sn : sn / cs;
}

// ---- atan, atan2, asin, acos --------------------------------------------------------------------------------

static const double ATAN_HI[4] = { 4.63647609000806093515e-01, 7.85398163397448278999e-01,
                                   9.82793723247329054082e-01, 1.57079632679489655800e+00 };
static const double ATAN_LO[4] = { 2.26987774529616870924e-17, 3.06161699786838301793e-17,
                                   1.39033110312309984516e-17, 6.12323399573676603587e-17 };
static const double ATAN_COEF[11] = { 3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01,
                               -1.11111104054623557880e-01, 9.09088713343650656196e-02, -7.69187620504482999495e-02,
                               6.66107313738753120669e-02, -5.83357013379057348645e-02, 4.97687799461593236017e-02,
                               -3.65315727442169155270e-02, 1.62858201153657823623e-02 };

double atan(double x)
{
    if (x != x)
        return x + x;
    int negative = signbit(x);
    double ax = fabs(x);
    if (ax >= 0x1p66)
        return x > 0.0 ? ATAN_HI[3] + ATAN_LO[3] : -ATAN_HI[3] - ATAN_LO[3];
    int id;
    if (ax < 0.4375)
    {
        if (ax < 0x1p-27)
            return x;
        id = -1;
    }
    else
    {
        x = ax;
        if (ax < 1.1875)
        {
            if (ax < 0.6875) { id = 0; x = (2.0 * x - 1.0) / (2.0 + x); }
            else { id = 1; x = (x - 1.0) / (x + 1.0); }
        }
        else
        {
            if (ax < 2.4375) { id = 2; x = (x - 1.5) / (1.0 + 1.5 * x); }
            else { id = 3; x = -1.0 / x; }
        }
    }
    double z = x * x;
    double w = z * z;
    double s1 = z * (ATAN_COEF[0] + w * (ATAN_COEF[2] + w * (ATAN_COEF[4] + w * (ATAN_COEF[6] + w * (ATAN_COEF[8] + w * ATAN_COEF[10])))));
    double s2 = w * (ATAN_COEF[1] + w * (ATAN_COEF[3] + w * (ATAN_COEF[5] + w * (ATAN_COEF[7] + w * ATAN_COEF[9]))));
    if (id < 0)
        return x - x * (s1 + s2);
    double r = ATAN_HI[id] - ((x * (s1 + s2) - ATAN_LO[id]) - x);
    return negative ? -r : r;
}

double atan2(double y, double x)
{
    if (x != x || y != y)
        return x + y;
    if (x == 1.0)
        return atan(y);
    int m = (signbit(y) ? 1 : 0) | (signbit(x) ? 2 : 0);
    if (y == 0.0)
    {
        switch (m)
        {
            case 0: case 1: return y;                    // atan(+-0, +anything) = +-0
            case 2: return PI_HI64;                      // atan(+0, -anything) = pi
            default: return -PI_HI64;                    // atan(-0, -anything) = -pi
        }
    }
    if (x == 0.0)
        return signbit(y) ? -PIO2_HI64 : PIO2_HI64;
    if (isinf(x))
    {
        if (isinf(y))
        {
            switch (m)
            {
                case 0: return 0.25 * PI_HI64;
                case 1: return -0.25 * PI_HI64;
                case 2: return 0.75 * PI_HI64;
                default: return -0.75 * PI_HI64;
            }
        }
        switch (m)
        {
            case 0: return 0.0;
            case 1: return -0.0;
            case 2: return PI_HI64;
            default: return -PI_HI64;
        }
    }
    if (isinf(y))
        return signbit(y) ? -PIO2_HI64 : PIO2_HI64;
    int k = ((__dhigh(y) & 0x7FF00000) - (__dhigh(x) & 0x7FF00000)) >> 20;
    double z;
    if (k > 60)
        z = PIO2_HI64 + 0.5 * PI_LO64;                   // |y/x| above 2^60
    else if (signbit(x) && k < -60)
        z = 0.0;                                         // |y|/x below -2^60
    else
        z = atan(fabs(y / x));
    switch (m)
    {
        case 0: return z;
        case 1: return -z;
        case 2: return PI_HI64 - (z - PI_LO64);
        default: return (z - PI_LO64) - PI_HI64;
    }
}

static const double PS0 = 1.66666666666666657415e-01;
static const double PS1 = -3.25565818622400915405e-01;
static const double PS2 = 2.01212532134862925881e-01;
static const double PS3 = -4.00555345006794114027e-02;
static const double PS4 = 7.91534994289814532176e-04;
static const double PS5 = 3.47933107596021167570e-05;
static const double QS1 = -2.40339491173441421878e+00;
static const double QS2 = 2.02094576023350569471e+00;
static const double QS3 = -6.88283971605453293030e-01;
static const double QS4 = 7.70381505559019352791e-02;

// (asin(x) - x) / x^3 as a rational function of t = x^2, for |x| <= 1/2.
static double asin_ratio(double t)
{
    double p = t * (PS0 + t * (PS1 + t * (PS2 + t * (PS3 + t * (PS4 + t * PS5)))));
    double q = 1.0 + t * (QS1 + t * (QS2 + t * (QS3 + t * QS4)));
    return p / q;
}

#define PIO4_HI64 7.85398163397448278999e-01

double asin(double x)
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
        return x * PIO2_HI64 + x * PIO2_LO64;
    if (ax < 0.5)
    {
        if (ax < 0x1p-27)
            return x;
        return x + x * asin_ratio(x * x);
    }
    // asin(x) = pi/2 - 2 asin(sqrt((1 - x)/2)).
    double t = (1.0 - ax) * 0.5;
    double r = asin_ratio(t);
    double s = sqrt(t);
    double result;
    if (ax >= 0.975)
        result = PIO2_HI64 - (2.0 * (s + s * r) - PIO2_LO64);
    else
    {
        double w = __dlow_cleared(s);
        double c = (t - w * w) / (s + w);
        double p = 2.0 * s * r - (PIO2_LO64 - 2.0 * c);
        double q = PIO4_HI64 - 2.0 * w;
        result = PIO4_HI64 - (p - q);
    }
    return x < 0.0 ? -result : result;
}

double acos(double x)
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
        return x > 0.0 ? 0.0 : PI_HI64 + 2.0 * PIO2_LO64;
    if (ax < 0.5)
    {
        if (ax <= 0x1p-57)
            return PIO2_HI64 + PIO2_LO64;
        double r = asin_ratio(x * x);
        return PIO2_HI64 - (x - (PIO2_LO64 - x * r));
    }
    if (x < 0.0)
    {
        double z = (1.0 + x) * 0.5;
        double s = sqrt(z);
        double w = asin_ratio(z) * s - PIO2_LO64;
        return PI_HI64 - 2.0 * (s + w);
    }
    double z = (1.0 - x) * 0.5;
    double s = sqrt(z);
    double df = __dlow_cleared(s);
    double c = (z - df * df) / (s + df);
    double w = asin_ratio(z) * s + c;
    return 2.0 * (df + w);
}
