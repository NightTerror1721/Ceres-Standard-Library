// 64-bit counts in two words. See ceres/ns64.h.
#include "ceres/ns64.h"

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
    struct ns64 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo ? 1u : 0u);      // the carry out of the low word
    return r;
}

struct ns64 ns64_sub(struct ns64 a, struct ns64 b)
{
    struct ns64 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo ? 1u : 0u);      // the borrow from the high word
    return r;
}

int ns64_cmp(struct ns64 a, struct ns64 b)
{
    if (a.hi != b.hi)
        return a.hi < b.hi ? -1 : 1;
    if (a.lo != b.lo)
        return a.lo < b.lo ? -1 : 1;
    return 0;
}

int ns64_is_zero(struct ns64 a)
{
    return a.lo == 0u && a.hi == 0u;
}

// x * y in full, from four 16-bit partial products: each fits a word, and the pieces are added with
// their carries by hand.
static struct ns64 mul_full(unsigned int x, unsigned int y)
{
    unsigned int xl = x & 0xFFFFu, xh = x >> 16;
    unsigned int yl = y & 0xFFFFu, yh = y >> 16;
    unsigned int ll = xl * yl;
    unsigned int lh = xl * yh;
    unsigned int hl = xh * yl;
    unsigned int hh = xh * yh;
    unsigned int mid = lh + hl;
    unsigned int mid_carry = mid < lh ? 1u : 0u;       // the middle sum is 33 bits wide
    struct ns64 r;
    r.lo = ll + (mid << 16);
    r.hi = hh + (mid >> 16) + (mid_carry << 16) + (r.lo < ll ? 1u : 0u);
    return r;
}

struct ns64 ns64_mul_u32(struct ns64 a, unsigned int m)
{
    struct ns64 r = mul_full(a.lo, m);
    r.hi += a.hi * m;                                  // the high word's product only matters modulo 2^32
    return r;
}

// Long division by 16 bits at a time: while d fits 16 bits, a remainder shifted up by 16 still fits a
// word and the machine's own division does each step.
static struct ns64 div_small(struct ns64 a, unsigned int d, unsigned int* rem)
{
    struct ns64 q;
    q.hi = a.hi / d;
    unsigned int r = a.hi % d;
    unsigned int t = (r << 16) | (a.lo >> 16);
    unsigned int q1 = t / d;
    r = t % d;
    t = (r << 16) | (a.lo & 0xFFFFu);
    unsigned int q0 = t / d;
    r = t % d;
    q.lo = (q1 << 16) | q0;
    *rem = r;
    return q;
}

// One bit at a time, for a divisor of more than 16 bits. The remainder can carry out of its word when it
// is shifted; that bit means the true remainder is beyond any divisor, so the divisor comes off.
static struct ns64 div_large(struct ns64 a, unsigned int d, unsigned int* rem)
{
    struct ns64 q = ns64_make(0u, 0u);
    unsigned int r = 0u;
    for (int i = 63; i >= 0; i--)
    {
        unsigned int bit = i >= 32 ? (a.hi >> (i - 32)) & 1u : (a.lo >> i) & 1u;
        unsigned int top = r >> 31;
        r = (r << 1) | bit;
        if (top != 0u || r >= d)
        {
            r -= d;
            if (i >= 32)
                q.hi |= 1u << (i - 32);
            else
                q.lo |= 1u << i;
        }
    }
    *rem = r;
    return q;
}

struct ns64 ns64_div_u32(struct ns64 a, unsigned int d, unsigned int* rem)
{
    unsigned int scratch;
    if (rem == 0)
        rem = &scratch;
    if (d == 0u)
    {
        *rem = 0u;
        return ns64_make(0xFFFFFFFFu, 0xFFFFFFFFu);
    }
    if (d <= 0xFFFFu)
        return div_small(a, d, rem);
    return div_large(a, d, rem);
}

struct ns64 ns64_from_us(unsigned int us)
{
    return mul_full(us, 1000u);
}

struct ns64 ns64_from_ms(unsigned int ms)
{
    return mul_full(ms, 1000000u);
}

struct ns64 ns64_from_sec(unsigned int sec)
{
    return mul_full(sec, 1000000000u);
}

// Divides by 1000 `steps` times, which is division by 10^(3*steps) without a divisor that needs the slow path.
static unsigned int to_unit(struct ns64 a, int steps)
{
    unsigned int rem;
    for (int i = 0; i < steps; i++)
        a = div_small(a, 1000u, &rem);
    return a.hi != 0u ? 0xFFFFFFFFu : a.lo;
}

unsigned int ns64_to_us(struct ns64 a)
{
    return to_unit(a, 1);
}

unsigned int ns64_to_ms(struct ns64 a)
{
    return to_unit(a, 2);
}

unsigned int ns64_to_sec(struct ns64 a)
{
    return to_unit(a, 3);
}

float ns64_to_float(struct ns64 a)
{
    // A word at a time from the top, sixteen bits per step, so nothing is converted that a float cannot hold.
    float f = (float)(a.hi >> 16);
    f = f * 65536.0f + (float)(a.hi & 0xFFFFu);
    f = f * 65536.0f + (float)(a.lo >> 16);
    f = f * 65536.0f + (float)(a.lo & 0xFFFFu);
    return f;
}
