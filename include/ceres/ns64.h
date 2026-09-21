#pragma once

// A 64-bit unsigned count, in two words: the machine is 32-bit and `long long` is not 64 bits here, but
// a count of nanoseconds needs them (32 bits of nanoseconds wrap every 4.29 seconds; 64 last 584 years).
// Meant for the clock - ceres/timer.h's timer_nanos() returns one - but it is plain arithmetic and knows
// nothing of the machine.
//
//   struct ns64 start = timer_nanos();
//   ...
//   struct ns64 spent = ns64_sub(timer_nanos(), start);
//   printf("%u us\n", ns64_to_us(spent));
//
// Add and subtract wrap at 2^64, so a difference is right whichever of the two readings wrapped. A
// conversion that does not fit 32 bits gives 0xFFFFFFFF rather than a number that is wrong.

struct ns64
{
    unsigned int lo;     // the low 32 bits
    unsigned int hi;     // the high 32 bits
};

_Static_assert(sizeof(struct ns64) == 8, "an ns64 is two words");

struct ns64 ns64_make(unsigned int lo, unsigned int hi);
struct ns64 ns64_from_u32(unsigned int v);

struct ns64 ns64_add(struct ns64 a, struct ns64 b);
struct ns64 ns64_sub(struct ns64 a, struct ns64 b);                       // a - b, modulo 2^64
int         ns64_cmp(struct ns64 a, struct ns64 b);                       // -1, 0 or 1, as unsigned counts
int         ns64_is_zero(struct ns64 a);

struct ns64 ns64_mul_u32(struct ns64 a, unsigned int m);                  // modulo 2^64
struct ns64 ns64_div_u32(struct ns64 a, unsigned int d, unsigned int* rem);   // rem may be 0; d == 0 gives the largest count

// From a span, and back. Going back rounds down.
struct ns64  ns64_from_us(unsigned int us);
struct ns64  ns64_from_ms(unsigned int ms);
struct ns64  ns64_from_sec(unsigned int sec);
unsigned int ns64_to_us(struct ns64 a);        // whole microseconds; saturates at 71 minutes
unsigned int ns64_to_ms(struct ns64 a);        // whole milliseconds; saturates at 49 days
unsigned int ns64_to_sec(struct ns64 a);       // whole seconds; saturates at 136 years
float        ns64_to_float(struct ns64 a);     // the count as a float: exact to about 7 digits, for a frame's dt and the like
