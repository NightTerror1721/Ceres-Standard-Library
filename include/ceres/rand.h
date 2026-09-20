#pragma once

#include "../stddef.h"

// Pseudo-random generators with explicit, reproducible state: two runs with the same seed give the same
// sequence (the VM is deterministic apart from its wall clock). The generator is xorshift128, seeded
// through splitmix32. Not cryptographic. stdlib's rand() is a separate, smaller generator.

struct rng { unsigned int s0, s1, s2, s3; };

void  rng_seed(struct rng* r, unsigned int seed);          // splitmix32 fills the state; any seed is fine, 0 included
void  rng_seed_entropy(struct rng* r);                     // from the wall clock and the tick counter
unsigned int rng_u32(struct rng* r);                       // 0 .. 2^32-1
int   rng_range(struct rng* r, int lo, int hi);            // lo..hi inclusive, without modulo bias; lo > hi swaps them
float rng_float(struct rng* r);                            // [0, 1), 24 random bits
int   rng_chance(struct rng* r, int percent);              // 1 with probability percent/100 (<= 0 never, >= 100 always)
void  rng_shuffle(struct rng* r, void* base, size_t n, size_t size);   // Fisher-Yates over n elements of `size` bytes

// A global generator for code that does not care which one it gets.
void         rand_seed(unsigned int seed);
unsigned int rand_u32(void);
int          rand_range(int lo, int hi);
