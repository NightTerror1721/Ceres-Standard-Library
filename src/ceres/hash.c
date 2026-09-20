// Hashes and checksums. See ceres/hash.h.
#include "ceres/hash.h"
#include "ceres/bits.h"

#include "hash_crc.inc"

unsigned int hash_fnv1a(const void* p, size_t n)
{
    const unsigned char* s = (const unsigned char*)p;
    unsigned int h = 0x811C9DC5u;
    for (size_t i = 0; i < n; i++)
    {
        h ^= s[i];
        h *= 16777619u;
    }
    return h;
}

unsigned int hash_djb2(const char* s)
{
    unsigned int h = 5381u;
    while (*s)
    {
        h = h * 33u + (unsigned char)*s;
        s++;
    }
    return h;
}

unsigned int hash_murmur3(const void* p, size_t n, unsigned int seed)
{
    const unsigned char* s = (const unsigned char*)p;
    unsigned int h = seed;
    size_t blocks = n / 4;
    for (size_t i = 0; i < blocks; i++)
    {
        // little-endian, assembled from bytes: the data need not be aligned
        unsigned int k = (unsigned int)s[4 * i] | ((unsigned int)s[4 * i + 1] << 8) |
                         ((unsigned int)s[4 * i + 2] << 16) | ((unsigned int)s[4 * i + 3] << 24);
        k *= 0xCC9E2D51u;
        k = bit_rotl(k, 15);
        k *= 0x1B873593u;
        h ^= k;
        h = bit_rotl(h, 13);
        h = h * 5u + 0xE6546B64u;
    }

    const unsigned char* tail = s + blocks * 4;
    unsigned int k = 0;
    switch (n & 3)
    {
    case 3: k ^= (unsigned int)tail[2] << 16;
    case 2: k ^= (unsigned int)tail[1] << 8;
    case 1:
        k ^= (unsigned int)tail[0];
        k *= 0xCC9E2D51u;
        k = bit_rotl(k, 15);
        k *= 0x1B873593u;
        h ^= k;
    }

    h ^= (unsigned int)n;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

unsigned int hash_crc32_update(unsigned int crc, const void* p, size_t n)
{
    const unsigned char* s = (const unsigned char*)p;
    unsigned int c = ~crc;
    for (size_t i = 0; i < n; i++)
        c = crc32_table[(c ^ s[i]) & 255u] ^ (c >> 8);
    return ~c;
}

unsigned int hash_crc32(const void* p, size_t n)
{
    return hash_crc32_update(0u, p, n);
}

unsigned int hash_adler32(const void* p, size_t n)
{
    const unsigned char* s = (const unsigned char*)p;
    unsigned int a = 1u, b = 0u;
    while (n > 0)
    {
        // 5552 is the most bytes that can be added before the sums could overflow 32 bits
        size_t chunk = n < 5552 ? n : 5552;
        n -= chunk;
        while (chunk-- > 0)
        {
            a += *s++;
            b += a;
        }
        a %= 65521u;
        b %= 65521u;
    }
    return (b << 16) | a;
}
