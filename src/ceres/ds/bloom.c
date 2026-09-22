// A Bloom filter over ceres/ds/bitset.h. See ceres/ds/bloom.h.
#include "ceres/ds/bloom.h"
#include "ceres/hash.h"

void bloom_init(struct bloom* b, unsigned int* words, unsigned int nbits, unsigned int k)
{
    bitset_init(&b->bits, words, nbits);
    b->k = k == 0 ? 1u : k;
}

void bloom_clear(struct bloom* b)
{
    bitset_clear_all(&b->bits);
}

// Kirsch-Mitzenmacher: k independent-enough hashes from two real ones, h1(x) + i * h2(x), instead
// of k separately seeded hash passes over the item.
static void two_hashes(const void* item, unsigned int len, unsigned int* h1, unsigned int* h2)
{
    *h1 = hash_fnv1a(item, len);
    *h2 = hash_murmur3(item, len, *h1);
    if ((*h2 & 1u) == 0)
        (*h2)++;                                            // odd step: visits every slot before repeating
}

void bloom_add(struct bloom* b, const void* item, unsigned int len)
{
    unsigned int h1, h2;
    two_hashes(item, len, &h1, &h2);
    for (unsigned int i = 0; i < b->k; i++)
        bitset_set(&b->bits, (h1 + i * h2) % b->bits.nbits);
}

int bloom_maybe_has(const struct bloom* b, const void* item, unsigned int len)
{
    unsigned int h1, h2;
    two_hashes(item, len, &h1, &h2);
    for (unsigned int i = 0; i < b->k; i++)
    {
        if (!bitset_get(&b->bits, (h1 + i * h2) % b->bits.nbits))
            return 0;
    }
    return 1;
}
