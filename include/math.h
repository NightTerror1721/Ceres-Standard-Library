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
union __float_bits { float f; unsigned int u; };
static inline float __fbits(unsigned int b) { union __float_bits x; x.u = b; return x.f; }
#define INFINITY   (__fbits(0x7F800000u))
#define NAN        (__fbits(0x7FC00000u))
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
extern float fmod(float x, float y);       // traps, like a division, when y == 0
extern float sqrt(float x);                // sqrt of a negative number is NaN (errno is not set)
extern float floor(float x);
extern float ceil(float x);
float round(float x);                      // to the nearest integer, halfway cases away from zero (src/math.c)
extern float trunc(float x);
extern float fmin(float x, float y);
extern float fmax(float x, float y);
extern float copysign(float x, float y);
extern float fma(float x, float y, float z);
extern float rcp(float x);                 // Ceres extensions: a fast approximation of 1/x ...
extern float rsqrt(float x);               // ... and of 1/sqrt(x)

// ---- in software (src/math.c) ----
float sin(float x);                        // |x| up to ~6000 is accurate; larger arguments lose digits
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
float cbrt(float x);
float hypot(float x, float y);             // sqrt(x*x + y*y) without overflow
float ldexp(float x, int e);               // x * 2^e
float frexp(float x, int* e);              // x = m * 2^e with 0.5 <= |m| < 1
float modf(float x, float* ip);            // splits into integer part (*ip) and fraction, both signed like x
float fdim(float x, float y);              // x - y when positive, else 0
float remainder(float x, float y);         // x - n*y with n the nearest integer (halves to even)
float nearbyint(float x);                  // to the nearest integer, halves to even: the `fround` instruction
float rint(float x);                       // same as nearbyint here (no inexact exception to raise)
int   lround(float x);                     // to the nearest int, halves away from zero
int   lrint(float x);                      // to the nearest int, halves to even
float nan(const char* tag);                // a quiet NaN (the tag is ignored)

#define scalbn    ldexp
#define llround   lround
#define llrint    lrint

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
