// fmod, on its own so that a program calling it links only this.
//
// The `fmod` instruction (__builtin_fmod) is the remainder itself, but a zero divisor makes it trap
// like a division: the Trap flag is set and x comes back unchanged. C wants a domain error there, and
// for an infinite x, with NaN as the result; an infinite y leaves x as it is, which the instruction
// already does. The cases are told apart with `fclass`, whose bits are listed in asm/math_ops.casm.
#include "math.h"
#include "errno.h"

#define CLASS_INFINITE 129u     // -inf, +inf
#define CLASS_ZERO      24u     // -0, +0
#define CLASS_NAN      256u

float fmod(float x, float y)
{
    unsigned int cx = (unsigned int)__builtin_fclass(x);
    unsigned int cy = (unsigned int)__builtin_fclass(y);
    if (((cx | cy) & CLASS_NAN) != 0)
        return NAN;                                  // NaN in, NaN out: no domain error of its own
    if ((cy & CLASS_ZERO) != 0 || (cx & CLASS_INFINITE) != 0)
    {
        errno = EDOM;
        return NAN;
    }
    return __builtin_fmod(x, y);
}
