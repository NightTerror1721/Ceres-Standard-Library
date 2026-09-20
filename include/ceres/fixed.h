#pragma once

// 16.16 fixed-point arithmetic, for games that want exact, repeatable numbers without the float unit.
// A fixed_t is a signed 32-bit integer holding value * 65536: the range is about -32768.0 .. 32767.99998
// and the step is 1/65536.

typedef int fixed_t;

#define FIX_SHIFT 16
#define FIX_ONE   65536
#define FIX_HALF  32768
#define FIX_MAX   0x7FFFFFFF
#define FIX_MIN   (-0x7FFFFFFF - 1)

#define FIX(x)        ((fixed_t)((x) * 65536.0f))    // from a float constant (truncates)
#define FIX_INT(i)    ((fixed_t)((i) << 16))         // from an integer
#define FIX_TO_INT(f) ((f) >> 16)                    // rounds toward minus infinity
#define FIX_FRAC(f)   ((f) & 0xFFFF)                 // the fraction, 0..65535

static inline fixed_t fx_add(fixed_t a, fixed_t b) { return a + b; }
static inline fixed_t fx_sub(fixed_t a, fixed_t b) { return a - b; }
static inline fixed_t fx_abs(fixed_t a) { return a < 0 ? -a : a; }
static inline fixed_t fx_floor(fixed_t a) { return a & ~0xFFFF; }
static inline fixed_t fx_round(fixed_t a) { return (a + FIX_HALF) & ~0xFFFF; }   // halves go up

fixed_t fx_mul(fixed_t a, fixed_t b);   // exact 64-bit product, then >> 16 (truncates); wraps on overflow
fixed_t fx_div(fixed_t a, fixed_t b);   // a / b; saturates on overflow, and b == 0 gives FIX_MAX or FIX_MIN by the sign of a
fixed_t fx_sqrt(fixed_t a);             // 0 for a <= 0; the result is truncated
fixed_t fx_lerp(fixed_t a, fixed_t b, fixed_t t);   // a + (b - a) * t, t as 0..1 in 16.16

// Angles are 0..255 for a whole turn (the value wraps: 256 is 0, -64 is 192), so 64 is a quarter turn.
fixed_t fx_sin(int angle);
fixed_t fx_cos(int angle);

float   fx_to_float(fixed_t a);
fixed_t fx_from_float(float f);         // truncates toward zero; saturates outside the range
