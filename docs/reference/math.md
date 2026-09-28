# `<math.h>`

<math.h> in the machine's two formats. The standard names take and give a double - IEEE binary64, computed with the 64-bit float instructions (fadd.d, fsqrt.d, fcvt...) - and the f-suffixed ones a float, the faster family: sinf, powf and the rest are the machine's binary32 and its own instructions. long double is double, so the l-suffixed names are the double functions. The standard names for the constants are M_*; the short internal ones (PI, LN2, ...) live in src/math_priv.h so they never collide with a user's own identifiers.

Domain and range errors set errno (EDOM / ERANGE) as C says and return NaN or +-infinity.

Accuracy of the doubles (src/math64*.c), measured against 80-bit and 400-digit references: exp, log, log2, log10, pow, cbrt, hypot, sin, cos, atan, asin, acos, expm1, log1p, erf and lgamma for x > 1/2 within about one unit in the last place, the argument reduction of sin, cos and tan exact for every double; tan, atan2, the hyperbolic functions and erfc within two or three; tgamma within five; lgamma for x < 0 keeps about 1e-16 absolute next to its zeros.

Accuracy of the floats (src/math.c, math_ext.c, math_special.c), measured against double references (tests/test_math.c): within about 2e-7 relative (one to three units in the last place) everywhere, except powf with a large exponent, whose error grows with |y*log(x)| (about 2e-6 at y = 100); sinf, cosf and tanf stay within 1.5 units (tanf 2.5) for every float.

A program compiled with -fshort-double, where double is float, gets the float family under the standard names.

```c
#ifdef __CERES_SHORT_DOUBLE__
#define sin        sinf
#define cos        cosf
#define tan        tanf
#define sincos     sincosf
#define asin       asinf
#define acos       acosf
#define atan       atanf
#define atan2      atan2f
#define sinh       sinhf
#define cosh       coshf
#define tanh       tanhf
#define asinh      asinhf
#define acosh      acoshf
#define atanh      atanhf
#define exp        expf
#define exp2       exp2f
#define expm1      expm1f
#define log        logf
#define log2       log2f
#define log10      log10f
#define log1p      log1pf
#define pow        powf
#define sqrt       sqrtf
#define cbrt       cbrtf
#define hypot      hypotf
#define fabs       fabsf
#define floor      floorf
#define ceil       ceilf
#define round      roundf
#define trunc      truncf
#define fmod       fmodf
#define fmin       fminf
#define fmax       fmaxf
#define copysign   copysignf
#define fma        fmaf
#define ldexp      ldexpf
#define frexp      frexpf
#define modf       modff
#define fdim       fdimf
#define remainder  remainderf
#define nearbyint  nearbyintf
#define rint       rintf
#define lround     lroundf
#define lrint      lrintf
#define llround    llroundf
#define llrint     llrintf
#define ilogb      ilogbf
#define logb       logbf
#define nextafter  nextafterf
#define erf        erff
#define erfc       erfcf
#define tgamma     tgammaf
#define lgamma     lgammaf
#define nan        nanf
#endif

#define M_E        2.718281828459045
#define M_LOG2E    1.4426950408889634
#define M_LOG10E   0.4342944819032518
#define M_LN2      0.6931471805599453
#define M_LN10     2.302585092994046
#define M_PI       3.141592653589793
#define M_PI_2     1.5707963267948966
#define M_PI_4     0.7853981633974483
#define M_1_PI     0.3183098861837907
#define M_2_PI     0.6366197723675814
#define M_2_SQRTPI 1.1283791670955126
#define M_SQRT2    1.4142135623730951
#define M_SQRT1_2  0.7071067811865476

// Infinity and NaN cannot be written as 1.0f/0.0f: a division by zero does not fail, it raises the Trap flag and
// leaves the destination untouched. Build them from their bit patterns: a float, which converts to double exactly.
#define INFINITY   (__builtin_float_from_bits(0x7F800000u))
#define NAN        (__builtin_float_from_bits(0x7FC00000u))
#define HUGE_VALF  INFINITY
#define HUGE_VAL   ((double)INFINITY)
#define HUGE_VALL  HUGE_VAL

typedef float float_t;                     // FLT_EVAL_METHOD is 0: float arithmetic is done in float
typedef double double_t;                   // ... and double arithmetic in double
#define FP_ILOGB0    (-2147483647 - 1)
#define FP_ILOGBNAN  (-2147483647 - 1)

// The reinterpretations of a float, one instruction each (asm/math_ops.casm).
unsigned int float_bits(float x);
float float_from_bits(unsigned int b);
```

## One instruction each (asm/math_ops.casm)

```c
double fabs(double x);
double sqrt(double x);                                     // sqrt of a negative number is NaN (errno is not set)
double floor(double x);
double ceil(double x);
double trunc(double x);
double rint(double x);                                     // to the nearest integer, halves to even: the `fround` instruction
double nearbyint(double x);                                // the same here (there is no inexact exception to raise)
double fmin(double x, double y);
double fmax(double x, double y);
double copysign(double x, double y);
double fma(double x, double y, double z);                  // x*y + z with one rounding
float fabsf(float x);
float sqrtf(float x);
float floorf(float x);
float ceilf(float x);
float truncf(float x);
float rintf(float x);
float nearbyintf(float x);
float fminf(float x, float y);
float fmaxf(float x, float y);
float copysignf(float x, float y);
float fmaf(float x, float y, float z);
```

## The rest

```c
double fmod(double x, double y);                           // NaN with errno = EDOM when y == 0 or x is infinite (src/fmod.c)
double round(double x);                                    // to the nearest integer, halfway cases away from zero
double sin(double x);
double cos(double x);
double tan(double x);
void sincos(double x, double* s, double* c);               // both at once, for the price of one argument reduction
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double exp(double x);
double exp2(double x);
double expm1(double x);                                    // exp(x) - 1, exact for tiny x
double log(double x);
double log2(double x);
double log10(double x);
double log1p(double x);                                    // log(1 + x), exact for tiny x
double pow(double x, double y);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double asinh(double x);
double acosh(double x);
double atanh(double x);
double cbrt(double x);
double hypot(double x, double y);                          // sqrt(x*x + y*y) without overflow
double ldexp(double x, int e);                             // x * 2^e
double frexp(double x, int* e);                            // x = m * 2^e with 0.5 <= |m| < 1
double modf(double x, double* ip);                         // splits into integer part (*ip) and fraction, both signed like x
double fdim(double x, double y);                           // x - y when positive, else 0
double remainder(double x, double y);                      // x - n*y with n the nearest integer (halves to even)
int lround(double x);                                      // to the nearest int, halves away from zero
int lrint(double x);                                       // to the nearest int, halves to even
long long llround(double x);                               // as lround, to the nearest 64-bit integer
long long llrint(double x);                                // as lrint, to the nearest 64-bit integer
int ilogb(double x);                                       // floor(log2|x|) as an int, subnormals too; FP_ILOGB0 for 0 and NaN, INT_MAX for inf (EDOM)
double logb(double x);                                     // ... as a floating value; -inf for 0 (ERANGE)
double nextafter(double x, double y);                      // the value next to x towards y; ERANGE when that overflows or is subnormal
double erf(double x);
double erfc(double x);                                     // 1 - erf(x) without the cancellation: right far into the tail
double tgamma(double x);                                   // EDOM at the negative integers and -inf, ERANGE at 0 and on overflow
double lgamma(double x);                                   // log|gamma(x)|; the sign of gamma(x) goes to signgam
double nan(const char* tag);                               // a quiet NaN (the tag is ignored)
float fmodf(float x, float y);
float roundf(float x);
float sinf(float x);
float cosf(float x);
float tanf(float x);
void sincosf(float x, float* s, float* c);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);
float expf(float x);
float exp2f(float x);
float expm1f(float x);
float logf(float x);
float log2f(float x);
float log10f(float x);
float log1pf(float x);
float powf(float x, float y);
float sinhf(float x);
float coshf(float x);
float tanhf(float x);
float asinhf(float x);
float acoshf(float x);
float atanhf(float x);
float cbrtf(float x);
float hypotf(float x, float y);
float ldexpf(float x, int e);
float frexpf(float x, int* e);
float modff(float x, float* ip);
float fdimf(float x, float y);
float remainderf(float x, float y);
int lroundf(float x);
int lrintf(float x);
long long llroundf(float x);
long long llrintf(float x);
int ilogbf(float x);
float logbf(float x);
float nextafterf(float x, float y);
float erff(float x);
float erfcf(float x);
float tgammaf(float x);
float lgammaf(float x);
float nanf(const char* tag);

float rcpf(float x);                       // Ceres extensions, float only: a fast approximation of 1/x ...
float rsqrtf(float x);                     // ... and of 1/sqrt(x)
#define rcp   rcpf
#define rsqrt rsqrtf
extern int signgam;

#define scalbn    ldexp
#define scalbnf   ldexpf
#define nexttoward(x, y)  nextafter((x), (double)(y))   // long double is double
#define nexttowardf(x, y) nextafterf((x), (float)(y))
```

## The long double names: long double is double

```c
#define sinl        sin
#define cosl        cos
#define tanl        tan
#define sincosl     sincos
#define asinl       asin
#define acosl       acos
#define atanl       atan
#define atan2l      atan2
#define sinhl       sinh
#define coshl       cosh
#define tanhl       tanh
#define asinhl      asinh
#define acoshl      acosh
#define atanhl      atanh
#define expl        exp
#define exp2l       exp2
#define expm1l      expm1
#define logl        log
#define log2l       log2
#define log10l      log10
#define log1pl      log1p
#define powl        pow
#define sqrtl       sqrt
#define cbrtl       cbrt
#define hypotl      hypot
#define fabsl       fabs
#define floorl      floor
#define ceill       ceil
#define roundl      round
#define truncl      trunc
#define fmodl       fmod
#define fminl       fmin
#define fmaxl       fmax
#define copysignl   copysign
#define fmal        fma
#define ldexpl      ldexp
#define frexpl      frexp
#define modfl       modf
#define fdiml       fdim
#define remainderl  remainder
#define nearbyintl  nearbyint
#define rintl       rint
#define lroundl     lround
#define lrintl      lrint
#define llroundl    llround
#define llrintl     llrint
#define ilogbl      ilogb
#define logbl       logb
#define nextafterl  nextafter
#define erfl        erf
#define erfcl       erfc
#define tgammal     tgamma
#define lgammal     lgamma
#define nanl        nan
#define nexttowardl nexttoward
#define scalbnl     ldexp
```

## The one-instruction ones, inline

```c
// Every function above that is one machine instruction, except fma, is also a macro on the compiler's builtin for
// it, so a call costs that instruction instead of a call, a return and the registers saved around them. A builtin
// converts nothing, so the argument is converted first, as the prototype would have done; a double argument makes
// the builtin the double instruction (fsqrt.d...), a float one the float one. The functions stay in
// asm/math_ops.casm for whoever takes their address or writes the name in parentheses: `(sqrt)(x)` is still a call.
#ifndef __CERES_SHORT_DOUBLE__
#define fabs(x)            __builtin_fabs((double)(x))
#define sqrt(x)            __builtin_sqrt((double)(x))
#define floor(x)           __builtin_floor((double)(x))
#define ceil(x)            __builtin_ceil((double)(x))
#define trunc(x)           __builtin_trunc((double)(x))
#define rint(x)            __builtin_rint((double)(x))
#define nearbyint(x)       __builtin_rint((double)(x))
#define fmin(x, y)         __builtin_fmin((double)(x), (double)(y))
#define fmax(x, y)         __builtin_fmax((double)(x), (double)(y))
#define copysign(x, y)     __builtin_copysign((double)(x), (double)(y))
#endif
#define fabsf(x)           __builtin_fabs((float)(x))
#define sqrtf(x)           __builtin_sqrt((float)(x))
#define floorf(x)          __builtin_floor((float)(x))
#define ceilf(x)           __builtin_ceil((float)(x))
#define truncf(x)          __builtin_trunc((float)(x))
#define rintf(x)           __builtin_rint((float)(x))
#define nearbyintf(x)      __builtin_rint((float)(x))
#define fminf(x, y)        __builtin_fmin((float)(x), (float)(y))
#define fmaxf(x, y)        __builtin_fmax((float)(x), (float)(y))
#define copysignf(x, y)    __builtin_copysign((float)(x), (float)(y))
#define rcpf(x)            __builtin_frcp((float)(x))
#define rsqrtf(x)          __builtin_frsqrt((float)(x))
#define float_bits(x)      __builtin_float_bits((float)(x))
#define float_from_bits(b) __builtin_float_from_bits((unsigned int)(b))
// fma and fmaf are not among them: Ceres-C has no __builtin_fma (the instruction accumulates into its destination,
// which its back end cannot guarantee a spare register for), so they stay calls.
```

## Classification

```c
// `fclass` sets exactly one bit: 0 -inf, 1 -normal, 2 -subnormal, 3 -0, 4 +0, 5 +subnormal, 6 +normal, 7 +inf,
// 8 NaN. A float is classified by the instruction; a double by the same bits worked out from its own (a double's
// subnormals are not a float's, so converting would get them wrong). Each macro evaluates its argument once and takes
// either type, as C's do.
// The functions behind the parenthesized forms - `(isnan)(x)` - are the float ones (asm/math_ops.casm).
int isnan(float x);
int isinf(float x);
int isfinite(float x);
int isnormal(float x);
int signbit(float x);
int fpclassify(float x);
#define FP_INFINITE   1
#define FP_NAN        2
#define FP_NORMAL     3
#define FP_SUBNORMAL  4
#define FP_ZERO       5
static inline int __fclass_double(double x)
{
    union { double d; unsigned long long u; } v;
    v.d = x;
    int negative = (int)(v.u >> 63);
    unsigned int exponent = (unsigned int)(v.u >> 52) & 0x7FFu;
    int fraction = (v.u & 0x000FFFFFFFFFFFFFull) != 0;
    if (exponent == 0x7FFu)
        return fraction ? 256 : (negative ? 1 : 128);
    if (exponent == 0)
        return fraction ? (negative ? 4 : 32) : (negative ? 8 : 16);
    return negative ? 2 : 64;
}
#define __fclass(x)   _Generic((x), float: __builtin_fclass((float)(x)), default: __fclass_double((double)(x)))
#define isnan(x)      ((__fclass(x) & 256) != 0)
#define isinf(x)      ((__fclass(x) & 129) != 0)
#define isfinite(x)   ((__fclass(x) & 385) == 0)
#define isnormal(x)   ((__fclass(x) & 66) != 0)
#define signbit(x)    ((__fclass(x) & 15) != 0)
static inline int __fpclassify_bits(int c)
{
    if ((c & 129) != 0) return FP_INFINITE;
    if ((c & 256) != 0) return FP_NAN;
    if ((c & 24) != 0)  return FP_ZERO;
    if ((c & 36) != 0)  return FP_SUBNORMAL;
    return FP_NORMAL;
}
#define fpclassify(x) __fpclassify_bits(__fclass(x))
```
