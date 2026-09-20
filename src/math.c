#include "math_priv.h"

#include "math.h"

// Software float functions. The single-instruction operations (fabs, fmod,
// sqrt, floor, ceil, round, trunc, fmin, fmax, copysign, fma, rcp, rsqrt) live
// in asm/math_ops.casm; everything here is built on those plus the ordinary
// + - * / operators the compiler already has.
//
// Precision target is float (24-bit mantissa, ~7 decimal digits): the
// polynomials below are Taylor minimax approximations on reduced ranges, good
// to a handful of ulps, which is plenty for a first stdlib.

float sin(float x)
{
    int neg = 0;
    if (x < 0.0f) { neg = 1; x = -x; }

    x = fmod(x, TWO_PI);                 // -> [0, 2pi)
    if (x > PI) x -= TWO_PI;             // -> [-pi, pi]
    if (x > HALF_PI) x = PI - x;         // (pi/2, pi] -> [0, pi/2)
    else if (x < -HALF_PI) x = -PI - x;  // [-pi, -pi/2) -> [0, pi/2)

    // sin(x) = x * P(x^2), P the Taylor minimax polynomial.
    float xx = x * x;
    float p = -0.0000000250521084f;   // -1/11!
    p = p * xx + 0.00000275573192f;   //  1/9!
    p = p * xx - 0.000198412698f;     // -1/7!
    p = p * xx + 0.00833333333f;      //  1/5!
    p = p * xx - 0.166666667f;        // -1/3!
    p = p * xx + 1.0f;
    float r = x * p;

    if (neg) r = -r;
    return r;
}

float cos(float x)
{
    return sin(x + HALF_PI);
}

float tan(float x)
{
    // Near a pole this divides by zero; the VM traps instead of faulting
    // (division-by-zero sets the Trap flag and leaves the result unchanged).
    return sin(x) / cos(x);
}

float atan(float x)
{
    int neg = 0, big = 0, more = 0;
    if (x < 0.0f) { x = -x; neg = 1; }
    if (x > 1.0f) { x = 1.0f / x; big = 1; }   // -> [0, 1]
    if (x > TAN_PI_8)                          // -> [0, tan(pi/8)]
    {
        x = (x - 1.0f) / (x + 1.0f);
        more = 1;
    }

    // atan(x) = x * P(x^2) on [0, tan(pi/8)].
    float xx = x * x;
    float p = 0.0769230769f;      //  1/13
    p = p * xx - 0.0909090909f;   // -1/11
    p = p * xx + 0.111111111f;    //  1/9
    p = p * xx - 0.142857143f;    // -1/7
    p = p * xx + 0.2f;            //  1/5
    p = p * xx - 0.333333333f;    // -1/3
    p = p * xx + 1.0f;
    float r = x * p;

    if (more) r = r + PI_4;
    if (big)  r = HALF_PI - r;
    if (neg)  r = -r;
    return r;
}

float atan2(float y, float x)
{
    if (x > 0.0f) return atan(y / x);
    if (x < 0.0f)
    {
        if (y >= 0.0f) return atan(y / x) + PI;
        return atan(y / x) - PI;
    }
    if (y > 0.0f) return HALF_PI;
    if (y < 0.0f) return -HALF_PI;
    return 0.0f;   // atan2(0, 0): undefined, return 0
}

float asin(float x)
{
    return atan2(x, sqrt(1.0f - x * x));   // |x| > 1 -> sqrt(negative) = NaN
}

float acos(float x)
{
    return atan2(sqrt(1.0f - x * x), x);
}

float exp(float x)
{
    // exp(x) = exp(x / 2^k) ^ (2^k): halve until the Taylor series converges
    // quickly, then square back up.
    int k = 0;
    while (x > LN2_HALF || x < -LN2_HALF)
    {
        x = x * 0.5f;
        k++;
    }

    float sum = 1.0f;
    float term = 1.0f;
    for (int i = 1; i <= 8; i++)
    {
        term = term * x / (float)i;
        sum = sum + term;
    }

    for (int i = 0; i < k; i++)
        sum = sum * sum;
    return sum;
}

float log(float x)
{
    if (x <= 0.0f)
        return 0.0f;   // domain error: log of a non-positive value

    // Reduce into [1/sqrt2, sqrt2], counting powers of two, then
    // log(x) = 2 * atanh((x-1)/(x+1)) + k * ln 2.
    int k = 0;
    while (x > SQRT2)    { x = x * 0.5f; k++; }
    while (x < INV_SQRT2) { x = x * 2.0f; k--; }

    float y = (x - 1.0f) / (x + 1.0f);
    float yy = y * y;
    float q = 0.111111111f;    // 1/9
    q = q * yy + 0.142857143f; // 1/7
    q = q * yy + 0.2f;         // 1/5
    q = q * yy + 0.333333333f; // 1/3
    q = q * yy + 1.0f;
    float atanh_y = y * q;

    return 2.0f * atanh_y + (float)k * LN2;
}

float log2(float x)
{
    return log(x) * LOG2E;
}

float log10(float x)
{
    return log(x) * LOG10E;
}

float pow(float x, float y)
{
    if (x == 0.0f)
    {
        if (y > 0.0f) return 0.0f;
        return 1.0f;   // 0^0 = 1; 0^negative would be +inf (simplified)
    }
    if (x < 0.0f)
    {
        // A negative base is only well-defined for an integer exponent.
        float r = exp(y * log(-x));
        if (trunc(y) == y)          // y is a whole number
        {
            int yi = (int)y;        // only safe for |y| within int range
            if (yi & 1) r = -r;     // odd exponent keeps the sign
        }
        return r;
    }
    return exp(y * log(x));
}
