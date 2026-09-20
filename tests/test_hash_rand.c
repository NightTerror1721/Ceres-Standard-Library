// Hashes against published vectors (and an independent implementation for the rest) and the random
// generators: reproducibility, the exact stream for a known seed, and the shape of the distribution.
#include "ceres/test.h"
#include "ceres/hash.h"
#include "ceres/rand.h"

#define H(s) ((unsigned int)hash_fnv1a(s, strlen(s)))

static void hashes(void)
{
    TEST_SECTION("fnv1a");
    CHECK_EQ(hash_fnv1a("", 0), 0x811C9DC5u);
    CHECK_EQ(hash_fnv1a("a", 1), 0xE40C292Cu);
    CHECK_EQ(H("foobar"), 0xBF9CF968u);
    CHECK_EQ(H("123456789"), 0xBB86B11Cu);

    TEST_SECTION("djb2");
    CHECK_EQ(hash_djb2(""), 5381u);
    CHECK_EQ(hash_djb2("a"), 0x0002B606u);
    CHECK_EQ(hash_djb2("abc"), 0x0B885C8Bu);
    CHECK_EQ(hash_djb2("The quick brown fox jumps over the lazy dog"), 0x34CC38DEu);
    CHECK_EQ(hash_str("abc"), hash_djb2("abc"));

    TEST_SECTION("murmur3");
    CHECK_EQ(hash_murmur3("", 0, 0), 0u);
    CHECK_EQ(hash_murmur3("", 0, 0x9747B28Cu), 0xEBB6C228u);
    CHECK_EQ(hash_murmur3("a", 1, 0), 0x3C2569B2u);            // one tail byte
    CHECK_EQ(hash_murmur3("abc", 3, 0), 0xB3DD93FAu);          // three tail bytes
    CHECK_EQ(hash_murmur3("123456789", 9, 0), 0xB4FEF382u);    // two blocks and a tail
    CHECK_EQ(hash_murmur3("Hello, world!", 13, 0x9747B28Cu), 0x24884CBAu);
    CHECK_EQ(hash_murmur3("The quick brown fox jumps over the lazy dog", 43, 0x9747B28Cu), 0x2FA826CDu);
    CHECK(hash_murmur3("abc", 3, 1) != hash_murmur3("abc", 3, 2));   // the seed matters

    TEST_SECTION("crc32");
    CHECK_EQ(hash_crc32("", 0), 0u);
    CHECK_EQ(hash_crc32("a", 1), 0xE8B7BE43u);
    CHECK_EQ(hash_crc32("123456789", 9), 0xCBF43926u);
    CHECK_EQ(hash_crc32("The quick brown fox jumps over the lazy dog", 43), 0x414FA339u);
    unsigned int running = 0;                                  // in pieces: the same as all at once
    running = hash_crc32_update(running, "1234", 4);
    running = hash_crc32_update(running, "", 0);
    running = hash_crc32_update(running, "56789", 5);
    CHECK_EQ(running, 0xCBF43926u);

    TEST_SECTION("adler32");
    CHECK_EQ(hash_adler32("", 0), 1u);
    CHECK_EQ(hash_adler32("a", 1), 0x00620062u);
    CHECK_EQ(hash_adler32("abc", 3), 0x024D0127u);
    CHECK_EQ(hash_adler32("Wikipedia", 9), 0x11E60398u);

    TEST_SECTION("longer input, and unaligned input");
    static unsigned char buf[1004];
    for (int i = 0; i < 1000; i++)
        buf[i] = (unsigned char)(i * 31 + 7);
    CHECK_EQ(hash_crc32(buf, 1000), 0x8902161Eu);
    CHECK_EQ(hash_adler32(buf, 1000), 0xD9F3F1BCu);
    CHECK_EQ(hash_fnv1a(buf, 1000), 0xA33B1355u);
    CHECK_EQ(hash_murmur3(buf, 1000, 42), 0x1E52DD21u);
    for (int shift = 1; shift <= 3; shift++)
    {
        for (int i = 0; i < 1000; i++)
            buf[i + shift] = (unsigned char)(i * 31 + 7);
        CHECK_EQ(hash_murmur3(buf + shift, 1000, 42), 0x1E52DD21u);
    }

    static unsigned char big[20000];                            // past the 5552-byte block adler defers its modulo over
    for (int i = 0; i < 20000; i++)
        big[i] = (unsigned char)(i * 7 + 3);
    CHECK_EQ(hash_adler32(big, 20000), 0x2800E92Bu);
}

static void streams(void)
{
    TEST_SECTION("the exact stream");
    struct rng r;
    rng_seed(&r, 0);
    CHECK_EQ(r.s0, 0x92CA2F0Eu);
    CHECK_EQ(r.s3, 0x4C081DBFu);
    CHECK_EQ(rng_u32(&r), 0x8F79F96Fu);
    CHECK_EQ(rng_u32(&r), 0x043B5A08u);
    CHECK_EQ(rng_u32(&r), 0xBC79BD5Eu);
    rng_seed(&r, 1);
    CHECK_EQ(rng_u32(&r), 0xE8570218u);
    CHECK_EQ(rng_u32(&r), 0x1E01BC81u);
    rng_seed(&r, 12345);
    CHECK_EQ(rng_u32(&r), 0x457223C5u);
    CHECK_EQ(rng_u32(&r), 0x63C8D4DDu);
    CHECK_EQ(rng_u32(&r), 0xA69ADE3Cu);
    CHECK_EQ(rng_u32(&r), 0x0267648Cu);
    CHECK_EQ(rng_u32(&r), 0xD6DF030Eu);

    TEST_SECTION("same seed, same sequence");
    struct rng a, b;
    rng_seed(&a, 777);
    rng_seed(&b, 777);
    int same = 1;
    for (int i = 0; i < 100; i++)
        if (rng_u32(&a) != rng_u32(&b))
            same = 0;
    CHECK(same);
    rng_seed(&b, 778);
    int differ = 0;
    for (int i = 0; i < 100; i++)
        if (rng_u32(&a) != rng_u32(&b))
            differ++;
    CHECK(differ > 95);                                         // another seed: an unrelated stream

    TEST_SECTION("the global generator");
    rand_seed(5);
    unsigned int g1 = rand_u32();
    unsigned int g2 = rand_u32();
    rand_seed(5);
    CHECK_EQ(rand_u32(), g1);
    CHECK_EQ(rand_u32(), g2);
    int inr = 1;
    for (int i = 0; i < 200; i++)
    {
        int v = rand_range(-3, 3);
        if (v < -3 || v > 3) inr = 0;
    }
    CHECK(inr);
}

static void ranges(void)
{
    struct rng r;
    rng_seed(&r, 2026);

    TEST_SECTION("range bounds");
    int ok = 1, hit_lo = 0, hit_hi = 0;
    for (int i = 0; i < 2000; i++)
    {
        int v = rng_range(&r, -5, 5);
        if (v < -5 || v > 5) ok = 0;
        if (v == -5) hit_lo = 1;
        if (v == 5) hit_hi = 1;
    }
    CHECK(ok);
    CHECK(hit_lo && hit_hi);                                    // both ends are reachable
    CHECK_EQ(rng_range(&r, 7, 7), 7);
    int swapped = 1;
    for (int i = 0; i < 100; i++)
    {
        int v = rng_range(&r, 10, 1);                           // lo > hi: the same as 1..10
        if (v < 1 || v > 10) swapped = 0;
    }
    CHECK(swapped);
    int wide = 1;
    for (int i = 0; i < 100; i++)
    {
        int v = rng_range(&r, -2000000000, 2000000000);
        if (v < -2000000000 || v > 2000000000) wide = 0;
    }
    CHECK(wide);
    rng_range(&r, -2147483647 - 1, 2147483647);                 // the whole int range: no division by zero

    TEST_SECTION("uniformity");
    int count[10];
    for (int i = 0; i < 10; i++) count[i] = 0;
    for (int i = 0; i < 10000; i++)
        count[rng_range(&r, 0, 9)]++;
    int chi = 0;                                                // sum of (count - 1000)^2 / 1000; 9 degrees of freedom
    for (int i = 0; i < 10; i++)
        chi += (count[i] - 1000) * (count[i] - 1000);
    chi /= 1000;
    CHECK(chi < 28);                                            // p = 0.001 is 27.9: a fair generator lands far below

    TEST_SECTION("no modulo bias");
    int low = 0, high = 0;                                      // a range that does not divide 2^32
    for (int i = 0; i < 30000; i++)
    {
        int v = rng_range(&r, 0, 2);
        if (v == 0) low++;
        if (v == 2) high++;
    }
    CHECK(low > 9600 && low < 10400);
    CHECK(high > 9600 && high < 10400);

    TEST_SECTION("float, chance, shuffle");
    float sum = 0.0f;
    int inside = 1;
    for (int i = 0; i < 5000; i++)
    {
        float f = rng_float(&r);
        if (f < 0.0f || f >= 1.0f) inside = 0;
        sum += f;
    }
    CHECK(inside);
    CHECK_NEAR(sum / 5000.0f, 0.5f, 0.02f);

    CHECK_EQ(rng_chance(&r, 0), 0);
    CHECK_EQ(rng_chance(&r, -5), 0);
    CHECK_EQ(rng_chance(&r, 100), 1);
    CHECK_EQ(rng_chance(&r, 250), 1);
    int hits = 0;
    for (int i = 0; i < 10000; i++)
        hits += rng_chance(&r, 30);
    CHECK(hits > 2750 && hits < 3250);

    int deck[10];
    for (int i = 0; i < 10; i++) deck[i] = i * i;
    rng_shuffle(&r, deck, 10, sizeof(int));
    int seen[10], moved = 0;
    for (int i = 0; i < 10; i++) seen[i] = 0;
    for (int i = 0; i < 10; i++)
    {
        for (int k = 0; k < 10; k++)
            if (deck[i] == k * k) seen[k]++;
        if (deck[i] != i * i) moved++;
    }
    int perm = 1;
    for (int i = 0; i < 10; i++) if (seen[i] != 1) perm = 0;
    CHECK(perm);                                                // every element exactly once
    CHECK(moved > 3);                                           // and not left where it was

    char bytes[5] = { 'a', 'b', 'c', 'd', 'e' };                // elements of one byte
    rng_shuffle(&r, bytes, 5, 1);
    int total = 0;
    for (int i = 0; i < 5; i++) total += bytes[i];
    CHECK_EQ(total, 'a' + 'b' + 'c' + 'd' + 'e');
    rng_shuffle(&r, bytes, 0, 1);                               // empty and single: nothing to do
    rng_shuffle(&r, bytes, 1, 1);

    TEST_SECTION("entropy");
    struct rng e1, e2;
    rng_seed_entropy(&e1);
    rng_seed_entropy(&e2);
    CHECK((e1.s0 | e1.s1 | e1.s2 | e1.s3) != 0);                // usable, whatever it is
    (void)rng_u32(&e2);
}

int main(void)
{
    hashes();
    streams();
    ranges();
    return test_summary();
}
