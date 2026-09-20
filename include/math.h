#pragma once

// double is float on this machine (there is no f64), so every function has ONE implementation.
// The standard names for the constants are M_*; the short internal ones (PI, LN2, ...) live in
// src/math_priv.h so they never collide with a user's own identifiers.

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
#define M_SQRT2    1.41421356f
#define M_SQRT1_2  0.707106781f

// Infinity and NaN cannot be written as 1.0f/0.0f: a float division by zero does not fail, it
// raises the Trap flag and leaves the destination untouched. Build them from their bit patterns.
union __float_bits { float f; unsigned int u; };
static inline float __fbits(unsigned int b) { union __float_bits x; x.u = b; return x.f; }
#define INFINITY   (__fbits(0x7F800000u))
#define NAN        (__fbits(0x7FC00000u))
#define HUGE_VAL   INFINITY

// The reinterpretations behind the above, one instruction each (asm/math_ops.casm).
unsigned int float_bits(float x);
float float_from_bits(unsigned int b);

// Classification, on the `fclass` instruction (asm/math_ops.casm).
int isnan(float x);
int isinf(float x);
int signbit(float x);

// One instruction each (asm/math_ops.casm).
extern float fabs(float x);
extern float fmod(float x, float y);       // traps, like a division, when y == 0
extern float sqrt(float x);
extern float floor(float x);
extern float ceil(float x);
extern float round(float x);
extern float trunc(float x);
extern float fmin(float x, float y);
extern float fmax(float x, float y);
extern float copysign(float x, float y);
extern float fma(float x, float y, float z);
extern float rcp(float x);
extern float rsqrt(float x);

// In software (src/math.c).
float sin(float x);
float cos(float x);
float tan(float x);
float asin(float x);
float acos(float x);
float atan(float x);
float atan2(float y, float x);
float exp(float x);
float log(float x);
float log2(float x);
float log10(float x);
float pow(float x, float y);
