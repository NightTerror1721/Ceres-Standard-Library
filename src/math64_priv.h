#pragma once
// Helpers private to the double functions (src/math64*.c). NOT installed in include/.
//
// A double is taken apart through a union with its 64 bits. The exact pieces - a product or a sum split into a
// rounded double and the part that rounding dropped - are Dekker's and Knuth's, in plain double arithmetic
// (Veltkamp's split does the product without a fused multiply-add). Several functions carry a value as such a
// pair ("double-double", hi + lo with |lo| no more than half an ulp of hi) to keep 20 or so bits more than a
// double holds, which is what lets log, pow and exp2 round to within about an ulp.

#include "math.h"

typedef unsigned long long __u64;

static inline __u64 __dbits(double x)
{
    union { double d; __u64 u; } v;
    v.d = x;
    return v.u;
}

static inline double __dfrom(__u64 u)
{
    union { double d; __u64 u; } v;
    v.u = u;
    return v.d;
}

// The high and low words, as fdlibm reads them.
static inline int __dhigh(double x) { return (int)(unsigned int)(__dbits(x) >> 32); }
static inline unsigned int __dlow(double x) { return (unsigned int)__dbits(x); }
static inline double __dwith_high(double x, int high)
{
    return __dfrom(((__u64)(unsigned int)high << 32) | (__dbits(x) & 0xFFFFFFFFull));
}
static inline double __dlow_cleared(double x) { return __dfrom(__dbits(x) & 0xFFFFFFFF00000000ull); }

// a + b exactly, as *hi + *lo: Knuth's two-sum, for any a and b.
static inline void __two_sum(double a, double b, double* hi, double* lo)
{
    double s = a + b;
    double bb = s - a;
    *lo = (a - (s - bb)) + (b - bb);
    *hi = s;
}

// The same when |a| >= |b| (or a is 0): Dekker's fast two-sum.
static inline void __fast_two_sum(double a, double b, double* hi, double* lo)
{
    double s = a + b;
    *lo = b - (s - a);
    *hi = s;
}

// a * b exactly, as *hi + *lo, for |a|, |b| below about 2^995 (Veltkamp's split of each into 26 + 27 bits).
static inline void __two_product(double a, double b, double* hi, double* lo)
{
    double p = a * b;
    double ca = 134217729.0 * a;                         // 2^27 + 1
    double ah = ca - (ca - a);
    double al = a - ah;
    double cb = 134217729.0 * b;
    double bh = cb - (cb - b);
    double bl = b - bh;
    *lo = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
    *hi = p;
}

// x * 2^n, with a result that is subnormal or out of range rounded the way one multiplication rounds it (musl's
// scalbn: the product is split so a subnormal result is rounded once, not twice).
double __scalbn64(double x, int n);

// e^(hi - lo) * 2^k for |hi - lo| up to about ln2/2, the argument in two parts; the nearest double but for
// values within a few hundredths of an ulp of a halfway point.
double __exp_kernel64(double hi, double lo, int k);

// 2^(hi + lo) for a pair from __log2_pair (or a plain double, lo = 0), finite and within -1100..1100; no errno.
double __exp2_pair(double hi, double lo);

// log2(x) and ln(x) as *hi + *lo, for a positive finite x (subnormals included), good to about 2^-64 relative.
void __log2_pair(double x, double* hi, double* lo);
void __log_pair(double x, double* hi, double* lo);

// x = n * pi/2 + (*y0 + *y1) with |y0 + y1| <= pi/4 (a hair more); returns n. Cody and Waite in three steps up to
// 2^19 * pi/2, Payne and Hanek past it (src/math64.c).
int __rem_pio2_64(double x, double* y0, double* y1);

// sin and cos of y0 + y1, |y0 + y1| <= pi/4 (fdlibm's kernels).
double __sin_kernel64(double x, double y);
double __cos_kernel64(double x, double y);

// ln 2, 1/ln 2, log10(2) and pi/2, each as a double and the double of the rest.
#define LN2_HI64      0.6931471805599453
#define LN2_LO64      2.3190468138462996e-17
#define INV_LN2_HI64  1.4426950408889634
#define INV_LN2_LO64  2.0355273740931033e-17
#define LOG10_2_HI64  0.3010299956639812
#define LOG10_2_LO64  (-2.8037281277851704e-18)
#define PIO2_HI64     1.5707963267948966
#define PIO2_LO64     6.123233995736766e-17
#define PI_HI64       3.141592653589793
#define PI_LO64       1.2246467991473532e-16
