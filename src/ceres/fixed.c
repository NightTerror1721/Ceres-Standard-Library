// 16.16 fixed point. fx_mul is in asm/bits.casm (imul + imulh). See ceres/fixed.h.
#include "ceres/fixed.h"

#include "fixed_sin.inc"

// a / b for 0 <= a and 0 < b as unsigned magnitudes; returns the 16.16 quotient, or 0xFFFFFFFF when it
// does not fit in 32 bits. The integer part comes from one hardware division; the sixteen fraction bits
// from a long division by shift and subtract (a 48-bit numerator would not fit a register).
static unsigned int div_magnitude(unsigned int a, unsigned int b)
{
    unsigned int whole = a / b;
    unsigned int rem = a % b;
    if (whole >= 0x10000u)
        return 0xFFFFFFFFu;
    unsigned int frac = 0;
    for (int i = 0; i < 16; i++)
    {
        unsigned int carry = rem >> 31;                 // rem * 2 may not fit: remember the bit that falls off
        rem <<= 1;
        frac <<= 1;
        if (carry || rem >= b)
        {
            rem -= b;
            frac |= 1u;
        }
    }
    return (whole << 16) | frac;
}

fixed_t fx_div(fixed_t a, fixed_t b)
{
    int negative = (a < 0) != (b < 0);
    if (b == 0)
        return a < 0 ? FIX_MIN : FIX_MAX;
    unsigned int ua = a < 0 ? 0u - (unsigned int)a : (unsigned int)a;
    unsigned int ub = b < 0 ? 0u - (unsigned int)b : (unsigned int)b;
    unsigned int q = div_magnitude(ua, ub);
    if (q > 0x7FFFFFFFu)
    {
        // -2^31 itself is representable, but only as a negative result
        if (negative && q == 0x80000000u)
            return FIX_MIN;
        return negative ? FIX_MIN : FIX_MAX;
    }
    return negative ? -(fixed_t)q : (fixed_t)q;
}

// floor(sqrt(a * 65536)) by the digit-by-digit method: two bits of the radicand per step, sixteen from a
// and then eight zero pairs for the fraction. The remainder stays below 2^25, so 32 bits are plenty.
fixed_t fx_sqrt(fixed_t a)
{
    if (a <= 0)
        return 0;
    unsigned int x = (unsigned int)a;
    unsigned int rem = 0, root = 0;
    for (int i = 0; i < 24; i++)
    {
        unsigned int pair = 0;
        if (i < 16)
        {
            pair = x >> 30;
            x <<= 2;
        }
        rem = (rem << 2) | pair;
        unsigned int trial = (root << 2) | 1u;
        root <<= 1;
        if (rem >= trial)
        {
            rem -= trial;
            root |= 1u;
        }
    }
    return (fixed_t)root;
}

fixed_t fx_lerp(fixed_t a, fixed_t b, fixed_t t)
{
    return a + fx_mul(b - a, t);
}

fixed_t fx_sin(int angle)
{
    unsigned int a = (unsigned int)angle & 255u;
    unsigned int quarter = a >> 6;                      // 0..3
    unsigned int i = a & 63u;
    switch (quarter)
    {
    case 0: return fx_sin_quarter[i];
    case 1: return fx_sin_quarter[64 - i];
    case 2: return -fx_sin_quarter[i];
    }
    return -fx_sin_quarter[64 - i];
}

fixed_t fx_cos(int angle)
{
    return fx_sin(angle + 64);
}

float fx_to_float(fixed_t a)
{
    return (float)a * 0.0000152587890625f;              // 2^-16
}

fixed_t fx_from_float(float f)
{
    if (f >= 32768.0f) return FIX_MAX;
    if (f <= -32768.0f) return FIX_MIN;
    return (fixed_t)(f * 65536.0f);
}
