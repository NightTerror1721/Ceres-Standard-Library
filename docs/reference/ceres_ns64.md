# `<ceres/ns64.h>`

A 64-bit unsigned count, in two words: the machine is 32-bit, but `long long` is a real 8-byte type and the arithmetic below runs on it. 32 bits of nanoseconds wrap every 4.29 seconds; 64 last 584 years. It was meant for the clock, and is plain arithmetic that knows nothing of the machine.

DEPRECATED: a uint64_t does all of this with the ordinary operators, and the timer reads one directly:

```c
uint64_t start = timer_nanos64();
...
uint64_t spent = timer_nanos64() - start;
```

(timer_nanos64, timer_cycles64). The struct keeps its two-word shape (the same layout a `uint64_t` has: low word first) so that code written before still builds and links; the timer no longer hands one out, and nothing in the library uses it any more.

Add and subtract wrap at 2^64, so a difference is right whichever of the two readings wrapped. A conversion that does not fit 32 bits gives 0xFFFFFFFF rather than a number that is wrong.

```c
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
```
