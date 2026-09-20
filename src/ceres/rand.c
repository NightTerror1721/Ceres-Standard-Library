// Explicit-state random generators. See ceres/rand.h.
#include "ceres/rand.h"
#include "ceres/timer.h"

// splitmix32: one step of a well-mixed sequence, used to spread a small seed over the whole state.
static unsigned int splitmix32(unsigned int* state)
{
    *state += 0x9E3779B9u;
    unsigned int z = *state;
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    return z ^ (z >> 16);
}

void rng_seed(struct rng* r, unsigned int seed)
{
    unsigned int s = seed;
    r->s0 = splitmix32(&s);
    r->s1 = splitmix32(&s);
    r->s2 = splitmix32(&s);
    r->s3 = splitmix32(&s);
    if ((r->s0 | r->s1 | r->s2 | r->s3) == 0)          // xorshift never leaves the all-zero state
        r->s0 = 1;
}

void rng_seed_entropy(struct rng* r)
{
    rng_seed(r, timer_clock() * 2654435761u ^ timer_ticks());
}

unsigned int rng_u32(struct rng* r)
{
    unsigned int t = r->s0 ^ (r->s0 << 11);
    r->s0 = r->s1;
    r->s1 = r->s2;
    r->s2 = r->s3;
    r->s3 = r->s3 ^ (r->s3 >> 19) ^ t ^ (t >> 8);
    return r->s3;
}

int rng_range(struct rng* r, int lo, int hi)
{
    if (lo > hi) { int t = lo; lo = hi; hi = t; }
    unsigned int span = (unsigned int)hi - (unsigned int)lo + 1u;
    if (span == 0u)                                    // the whole 32-bit range
        return (int)rng_u32(r);
    // Values below 2^32 mod span would make the low results more likely: throw them away.
    unsigned int threshold = (0u - span) % span;
    unsigned int x = rng_u32(r);
    while (x < threshold)
        x = rng_u32(r);
    return (int)((unsigned int)lo + x % span);
}

float rng_float(struct rng* r)
{
    return (float)(int)(rng_u32(r) >> 8) * 0.000000059604645f;     // 2^-24: the 24 bits fill a float mantissa exactly
}

int rng_chance(struct rng* r, int percent)
{
    if (percent <= 0) return 0;
    if (percent >= 100) return 1;
    return rng_range(r, 0, 99) < percent;
}

void rng_shuffle(struct rng* r, void* base, size_t n, size_t size)
{
    char* a = (char*)base;
    for (size_t i = n; i > 1; i--)
    {
        size_t j = (size_t)rng_range(r, 0, (int)i - 1);
        if (j == i - 1) continue;
        char* p = a + (i - 1) * size;
        char* q = a + j * size;
        for (size_t k = 0; k < size; k++)
        {
            char t = p[k];
            p[k] = q[k];
            q[k] = t;
        }
    }
}

// ---- the global generator ----

static struct rng global_rng = { 0x9E3779B9u, 0x243F6A88u, 0xB7E15162u, 0xDEADBEEFu };

void rand_seed(unsigned int seed) { rng_seed(&global_rng, seed); }
unsigned int rand_u32(void) { return rng_u32(&global_rng); }
int rand_range(int lo, int hi) { return rng_range(&global_rng, lo, hi); }
