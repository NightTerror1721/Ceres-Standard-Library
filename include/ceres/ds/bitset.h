#pragma once

#include "../bits.h"

// A set of small integers 0 .. nbits-1 as one bit each, over words the caller provides: occupancy maps,
// tile flags, "which of these are used". The single-bit operations are inline and do not check the index;
// bitset_words() says how many words a set of n bits needs.

struct bitset
{
    unsigned int* w;
    unsigned int nbits;
};

static inline unsigned int bitset_words(unsigned int nbits) { return (nbits + 31u) >> 5; }

void bitset_init(struct bitset* s, unsigned int* words, unsigned int nbits);   // every bit clear
void bitset_clear_all(struct bitset* s);
void bitset_set_all(struct bitset* s);                              // sets bits 0 .. nbits-1 and no others

static inline int  bitset_get(const struct bitset* s, unsigned int i)    { return (s->w[i >> 5] >> (i & 31u)) & 1u; }
static inline void bitset_set(struct bitset* s, unsigned int i)          { s->w[i >> 5] |= 1u << (i & 31u); }
static inline void bitset_clear(struct bitset* s, unsigned int i)        { s->w[i >> 5] &= ~(1u << (i & 31u)); }
static inline void bitset_toggle(struct bitset* s, unsigned int i)       { s->w[i >> 5] ^= 1u << (i & 31u); }

unsigned int bitset_count(const struct bitset* s);                  // how many bits are set
int bitset_find_first_set(const struct bitset* s);                  // the lowest set bit, or -1 when there is none
int bitset_find_first_clear(const struct bitset* s);                // the lowest clear bit below nbits, or -1 when every bit is set
