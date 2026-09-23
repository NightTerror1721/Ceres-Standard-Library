// 64-bit counts. See ceres/ns64.h. The arithmetic runs on the real 64-bit integer type the compiler
// lowers as a word pair; the struct stays the public shape so existing callers keep working.
#include "ceres/ns64.h"
#include "stdint.h"

// The two public words and the 64-bit value have the same layout: low word at offset 0, high at 4.
static uint64_t to_u64(struct ns64 a)
{
    return ((uint64_t)a.hi << 32) | (uint64_t)a.lo;
}

static struct ns64 from_u64(uint64_t v)
{
    struct ns64 r;
    r.lo = (unsigned int)v;
    r.hi = (unsigned int)(v >> 32);
    return r;
}

struct ns64 ns64_make(unsigned int lo, unsigned int hi)
{
    struct ns64 r;
    r.lo = lo;
    r.hi = hi;
    return r;
}

struct ns64 ns64_from_u32(unsigned int v)
{
    return ns64_make(v, 0u);
}

struct ns64 ns64_add(struct ns64 a, struct ns64 b)
{
    return from_u64(to_u64(a) + to_u64(b));       // wraps at 2^64, as the caller expects
}

struct ns64 ns64_sub(struct ns64 a, struct ns64 b)
{
    return from_u64(to_u64(a) - to_u64(b));       // a - b, modulo 2^64
}

int ns64_cmp(struct ns64 a, struct ns64 b)
{
    uint64_t x = to_u64(a), y = to_u64(b);
    return x == y ? 0 : (x < y ? -1 : 1);         // unsigned counts
}

int ns64_is_zero(struct ns64 a)
{
    return to_u64(a) == 0ULL;
}

struct ns64 ns64_mul_u32(struct ns64 a, unsigned int m)
{
    return from_u64(to_u64(a) * (uint64_t)m);     // modulo 2^64
}

struct ns64 ns64_div_u32(struct ns64 a, unsigned int d, unsigned int* rem)
{
    uint64_t v = to_u64(a);
    if (d == 0u)
    {
        if (rem != 0)
            *rem = 0u;
        return ns64_make(0xFFFFFFFFu, 0xFFFFFFFFu);   // the largest count, as before
    }
    if (rem != 0)
        *rem = (unsigned int)(v % (uint64_t)d);
    return from_u64(v / (uint64_t)d);
}

struct ns64 ns64_from_us(unsigned int us)  { return from_u64((uint64_t)us * 1000ULL); }
struct ns64 ns64_from_ms(unsigned int ms)  { return from_u64((uint64_t)ms * 1000000ULL); }
struct ns64 ns64_from_sec(unsigned int sec){ return from_u64((uint64_t)sec * 1000000000ULL); }

// Divides by 1000 `steps` times, then saturates at 0xFFFFFFFF when the result still has a high word.
static unsigned int to_unit(struct ns64 a, int steps)
{
    uint64_t v = to_u64(a);
    for (int i = 0; i < steps; i++)
        v /= 1000ULL;
    return (v >> 32) != 0ULL ? 0xFFFFFFFFu : (unsigned int)v;
}

unsigned int ns64_to_us(struct ns64 a)  { return to_unit(a, 1); }
unsigned int ns64_to_ms(struct ns64 a)  { return to_unit(a, 2); }
unsigned int ns64_to_sec(struct ns64 a) { return to_unit(a, 3); }

float ns64_to_float(struct ns64 a)
{
    return (float)to_u64(a);                      // the compiler's own 64-bit -> float conversion
}
