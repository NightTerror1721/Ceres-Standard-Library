// Bit sets. See ceres/ds/bitset.h.
#include "ceres/ds/bitset.h"

void bitset_init(struct bitset* s, unsigned int* words, unsigned int nbits)
{
    s->w = words;
    s->nbits = nbits;
    bitset_clear_all(s);
}

void bitset_clear_all(struct bitset* s)
{
    unsigned int words = bitset_words(s->nbits);
    for (unsigned int i = 0; i < words; i++)
        s->w[i] = 0;
}

void bitset_set_all(struct bitset* s)
{
    unsigned int words = bitset_words(s->nbits);
    for (unsigned int i = 0; i < words; i++)
        s->w[i] = 0xFFFFFFFFu;
    unsigned int tail = s->nbits & 31u;
    if (tail != 0)                                      // the bits past the end of the last word stay clear
        s->w[words - 1] = (1u << tail) - 1u;
}

unsigned int bitset_count(const struct bitset* s)
{
    unsigned int words = bitset_words(s->nbits);
    unsigned int n = 0;
    for (unsigned int i = 0; i < words; i++)
        n += bit_popcount(s->w[i]);
    return n;
}

int bitset_find_first_set(const struct bitset* s)
{
    unsigned int words = bitset_words(s->nbits);
    for (unsigned int i = 0; i < words; i++)
    {
        if (s->w[i] != 0)
        {
            unsigned int bit = i * 32u + bit_ctz(s->w[i]);
            return bit < s->nbits ? (int)bit : -1;
        }
    }
    return -1;
}

int bitset_find_first_clear(const struct bitset* s)
{
    unsigned int words = bitset_words(s->nbits);
    for (unsigned int i = 0; i < words; i++)
    {
        if (s->w[i] != 0xFFFFFFFFu)
        {
            unsigned int bit = i * 32u + bit_ctz(~s->w[i]);
            return bit < s->nbits ? (int)bit : -1;      // the free bit may lie past the end of the last word
        }
    }
    return -1;
}
