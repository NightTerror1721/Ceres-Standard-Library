#pragma once
// Constants and helpers private to src/math.c and src/math_ext.c. NOT installed in include/: the
// short names (PI, LN2...) would collide with the user's own identifiers.

#include "math.h"

#define PI         3.14159265f
#define TWO_PI     6.28318531f
#define HALF_PI    1.57079633f
#define PI_4       0.785398163f
#define LN2        0.693147181f
#define LN10       2.30258509f
#define LOG2E      1.44269504f
#define LOG10E     0.434294482f
#define LOG10_2    0.301029996f
#define SQRT2      1.41421356f
#define INV_SQRT2  0.707106781f
#define TAN_PI_8   0.414213562f
#define LN2_HALF   0.34657359f
#define TWO_OVER_PI 0.636619772f

// ln 2 in two parts, as in Cephes expf: LN2_C1 has few significant bits, so k * LN2_C1 is exact for
// any k that matters, and x - k * LN2_C1 - k * LN2_C2 loses nothing to cancellation.
#define LN2_C1     0.693359375f
#define LN2_C2     (-2.12194440e-4f)

// pi/2 in three parts of 12, 12 and 24 bits: n * PIO2_1 and n * PIO2_2 are exact for |n| < 4096
// (arguments up to about 6400), which is what keeps sin(3.1415927f) correct to its last digit.
#define PIO2_1     1.5703125f
#define PIO2_2     4.8375129699707031e-4f
#define PIO2_3     7.54979013e-8f

float __exp_shift(float x, int shift);    // exp(x) * 2^shift (src/math.c)

// x * 2^n, the multiplication done by exact powers of two so a result that is subnormal or too big
// rounds the way the hardware rounds it. n may be anything an int can hold; the ends are clamped.
static inline float __scale2(float x, int n)
{
    if (n > 300) n = 300;
    if (n < -300) n = -300;
    while (n > 127)
    {
        x = x * 1.70141183e38f;                        // 2^127
        n -= 127;
    }
    while (n < -126)
    {
        x = x * 1.17549435e-38f;                       // 2^-126
        n += 126;
    }
    return x * float_from_bits((unsigned int)(n + 127) << 23);
}
