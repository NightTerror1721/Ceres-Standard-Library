#pragma once

// double is float on this machine (there is no f64), so every function has ONE implementation and
// the f-suffixed C99 names (sinf, powf, ...) are aliases. The standard names for the constants are
// M_*; the short internal ones (PI, LN2, ...) live in src/math_priv.h so they never collide with a
// user's own identifiers.
//
// Domain and range errors set errno (EDOM / ERANGE) as C says and return NaN or +-infinity. Accuracy,
// measured against double-precision references (tests/test_math.c): within about 2e-7 relative (one
// to three units in the last place) everywhere, except pow with a large exponent, whose error grows
// with |y*log(x)| (about 2e-6 at y = 100), and sin/cos/tan, which lose digits for |x| above ~6000.

#define M_E        2.71828183f
#define M_LOG2E    1.44269504f
#define M_LOG10E   0.434294482f
#define M_LN2      0.693147181f
#define M_LN10     2.30258509f
#define M_PI       3.14159265f
#define M_PI_2     1.57079633f
#define M_PI_4     0.785398163f
#define M_1_PI     0.318309886f
#define M_2_PI     0.636619772f
#define M_2_SQRTPI 1.12837917f
#define M_SQRT2    1.41421356f
#define M_SQRT1_2  0.707106781f

// Infinity and NaN cannot be written as 1.0f/0.0f: a float division by zero does not fail, it
// raises the Trap flag and leaves the destination untouched. Build them from their bit patterns.
#define INFINITY   (__builtin_float_from_bits(0x7F800000u))
#define NAN        (__builtin_float_from_bits(0x7FC00000u))
#define HUGE_VAL   INFINITY
#define HUGE_VALF  INFINITY

// The reinterpretations behind the above, one instruction each (asm/math_ops.casm).
unsigned int float_bits(float x);
float float_from_bits(unsigned int b);

// ---- classification, on the `fclass` instruction (asm/math_ops.casm) ----
#define FP_INFINITE   1
#define FP_NAN        2
#define FP_NORMAL     3
#define FP_SUBNORMAL  4
#define FP_ZERO       5
int fpclassify(float x);
int isnan(float x);
int isinf(float x);
int isfinite(float x);
int isnormal(float x);
int signbit(float x);

// ---- one instruction each (asm/math_ops.casm) ----
extern float fabs(float x);
float fmod(float x, float y);              // NaN with errno = EDOM when y == 0 or x is infinite (src/fmod.c)
extern float sqrt(float x);                // sqrt of a negative number is NaN (errno is not set)
extern float floor(float x);
extern float ceil(float x);
float round(float x) __attribute__((__const__));   // to the nearest integer, halfway cases away from zero (src/math.c)
extern float trunc(float x);
extern float fmin(float x, float y);
extern float fmax(float x, float y);
extern float copysign(float x, float y);
extern float fma(float x, float y, float z);
extern float rcp(float x);                 // Ceres extensions: a fast approximation of 1/x ...
extern float rsqrt(float x);               // ... and of 1/sqrt(x)

// ---- in software (src/math.c) ----
float sin(float x);                        // within 1.5 units in the last place for every float (tan: 2.5)
float cos(float x);
float tan(float x);
void  sincos(float x, float* s, float* c); // both at once, for the price of one argument reduction
float asin(float x);
float acos(float x);
float atan(float x);
float atan2(float y, float x);
float exp(float x);
float exp2(float x);
float log(float x);
float log2(float x);
float log10(float x);
float pow(float x, float y);

// ---- in software (src/math_ext.c) ----
float sinh(float x);
float cosh(float x);
float tanh(float x);
float asinh(float x);
float acosh(float x);
float atanh(float x);
float expm1(float x);                      // exp(x) - 1, exact for tiny x
float log1p(float x);                      // log(1 + x), exact for tiny x
float cbrt(float x) __attribute__((__const__));
float hypot(float x, float y);             // sqrt(x*x + y*y) without overflow
float ldexp(float x, int e);               // x * 2^e
float frexp(float x, int* e);              // x = m * 2^e with 0.5 <= |m| < 1
float modf(float x, float* ip);            // splits into integer part (*ip) and fraction, both signed like x
float fdim(float x, float y) __attribute__((__const__));   // x - y when positive, else 0
float remainder(float x, float y);         // x - n*y with n the nearest integer (halves to even)
float nearbyint(float x);                  // to the nearest integer, halves to even: the `fround` instruction
float rint(float x);                       // same as nearbyint here (no inexact exception to raise)
int   lround(float x);                     // to the nearest int, halves away from zero
int   lrint(float x);                      // to the nearest int, halves to even
long long llround(float x);                // as lround, to the nearest 64-bit integer
long long llrint(float x);                 // as lrint, to the nearest 64-bit integer
float nan(const char* tag) __attribute__((__const__));   // a quiet NaN (the tag is ignored)

// ---- the exponent, and the next float (src/math_ext.c) ----
typedef float float_t;                     // FLT_EVAL_METHOD is 0: float arithmetic is done in float
typedef float double_t;                    // ... and double is float
#define FP_ILOGB0    (-2147483647 - 1)
#define FP_ILOGBNAN  (-2147483647 - 1)
int   ilogb(float x);                      // floor(log2|x|) as an int, subnormals too; FP_ILOGB0 for 0 and NaN, INT_MAX for inf (EDOM)
float logb(float x);                       // ... as a float; -inf for 0 (ERANGE)
float nextafter(float x, float y);         // the float next to x towards y; ERANGE when that overflows or is subnormal
#define nexttoward(x, y) nextafter((x), (float)(y))   // long double is float too

// ---- the error and gamma functions (src/math_special.c): erf within 2 units in the last place, erfc and tgamma 4,
// lgamma 3 - except next to the zeros it has between the poles for x < 0, where it keeps about 6e-7 absolute ----
float erf(float x);
float erfc(float x);                       // 1 - erf(x) without the cancellation: right far into the tail
float tgamma(float x);                     // EDOM at the negative integers and -inf, ERANGE at 0 and past 35.04
float lgamma(float x);                     // log|gamma(x)|; the sign of gamma(x) goes to signgam
extern int signgam;

#define scalbn    ldexp

// ---- the one-instruction ones, inline ----
// Every function above that is one machine instruction, except fma (below), is also a macro on the
// compiler's builtin for it, so a call costs that instruction instead of a call, a return and the registers saved
// around them. The argument is converted to float first, as the prototype would have done (a
// builtin converts nothing). The functions stay in asm/math_ops.casm for whoever takes their
// address or writes the name in parentheses: `(sqrt)(x)` is still a call.
#define fabs(x)            __builtin_fabs((float)(x))
#define sqrt(x)            __builtin_sqrt((float)(x))
#define floor(x)           __builtin_floor((float)(x))
#define ceil(x)            __builtin_ceil((float)(x))
#define trunc(x)           __builtin_trunc((float)(x))
#define rint(x)            __builtin_rint((float)(x))
#define nearbyint(x)       __builtin_rint((float)(x))
#define fmin(x, y)         __builtin_fmin((float)(x), (float)(y))
#define fmax(x, y)         __builtin_fmax((float)(x), (float)(y))
#define copysign(x, y)     __builtin_copysign((float)(x), (float)(y))
#define rcp(x)             __builtin_frcp((float)(x))
#define rsqrt(x)           __builtin_frsqrt((float)(x))
#define float_bits(x)      __builtin_float_bits((float)(x))
#define float_from_bits(b) __builtin_float_from_bits((unsigned int)(b))
// fma is not among them: Ceres-C has no __builtin_fma (the instruction accumulates into its
// destination, which its back end cannot guarantee a spare register for), so fma stays a call.

// Classification on `fclass`, which sets exactly one bit: 0 -inf, 1 -normal, 2 -subnormal, 3 -0,
// 4 +0, 5 +subnormal, 6 +normal, 7 +inf, 8 NaN. Each evaluates its argument once.
#define isnan(x)      ((__builtin_fclass((float)(x)) & 256) != 0)
#define isinf(x)      ((__builtin_fclass((float)(x)) & 129) != 0)
#define isfinite(x)   ((__builtin_fclass((float)(x)) & 385) == 0)
#define isnormal(x)   ((__builtin_fclass((float)(x)) & 66) != 0)
#define signbit(x)    ((__builtin_fclass((float)(x)) & 15) != 0)
static inline int __fpclassify_bits(int c)
{
    if ((c & 129) != 0) return FP_INFINITE;
    if ((c & 256) != 0) return FP_NAN;
    if ((c & 24) != 0)  return FP_ZERO;
    if ((c & 36) != 0)  return FP_SUBNORMAL;
    return FP_NORMAL;
}
#define fpclassify(x) __fpclassify_bits(__builtin_fclass((float)(x)))

// ---- the f-suffixed names ----
#define sinf sin
#define cosf cos
#define tanf tan
#define asinf asin
#define acosf acos
#define atanf atan
#define atan2f atan2
#define sinhf sinh
#define coshf cosh
#define tanhf tanh
#define asinhf asinh
#define acoshf acosh
#define atanhf atanh
#define expf exp
#define exp2f exp2
#define expm1f expm1
#define logf log
#define log2f log2
#define log10f log10
#define log1pf log1p
#define powf pow
#define sqrtf sqrt
#define cbrtf cbrt
#define hypotf hypot
#define fabsf fabs
#define floorf floor
#define ceilf ceil
#define roundf round
#define truncf trunc
#define fmodf fmod
#define fminf fmin
#define fmaxf fmax
#define copysignf copysign
#define fmaf fma
#define ldexpf ldexp
#define frexpf frexp
#define modff modf
#define fdimf fdim
#define remainderf remainder
#define nearbyintf nearbyint
#define rintf rint
#define lroundf lround
#define lrintf lrint
#define llroundf llround
#define llrintf llrint
#define ilogbf ilogb
#define logbf logb
#define nextafterf nextafter
#define nexttowardf nexttoward
#define erff erf
#define erfcf erfc
#define tgammaf tgamma
#define lgammaf lgamma
