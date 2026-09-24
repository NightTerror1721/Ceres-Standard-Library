#include "ceres/test.h"
#include "ceres/bits.h"

int main(void)
{
    TEST_SECTION("clz / ctz");
    CHECK_EQ((int)bit_clz(1), 31);
    CHECK_EQ((int)bit_clz(0x80000000u), 0);
    CHECK_EQ((int)bit_clz(0), 32);
    CHECK_EQ((int)bit_clz(0x00010000u), 15);
    CHECK_EQ((int)bit_ctz(8), 3);
    CHECK_EQ((int)bit_ctz(1), 0);
    CHECK_EQ((int)bit_ctz(0x80000000u), 31);
    CHECK_EQ((int)bit_ctz(0), 32);

    TEST_SECTION("popcount / bswap");
    CHECK_EQ((int)bit_popcount(0), 0);
    CHECK_EQ((int)bit_popcount(0xF0F0u), 8);
    CHECK_EQ((int)bit_popcount(0xFFFFFFFFu), 32);
    CHECK(bit_bswap(0x11223344u) == 0x44332211u);
    CHECK(bit_bswap(bit_bswap(0xDEADBEEFu)) == 0xDEADBEEFu);

    TEST_SECTION("rotations");
    CHECK(bit_rotl(0x80000001u, 4) == 0x18u);
    CHECK(bit_rotr(0x18u, 4) == 0x80000001u);
    CHECK(bit_rotl(0x12345678u, 0) == 0x12345678u);
    CHECK(bit_rotl(0x12345678u, 32) == 0x12345678u);     // the count uses its low five bits
    CHECK(bit_rotl(1u, 31) == 0x80000000u);

    TEST_SECTION("wide multiply");
    CHECK(umulhi(0xFFFFFFFFu, 0xFFFFFFFFu) == 0xFFFFFFFEu);
    CHECK(umulhi(0x10000u, 0x10000u) == 1u);
    CHECK(umulhi(3u, 5u) == 0u);
    CHECK_EQ(imulhi(-1, 1), -1);                          // -1 * 1 = 0xFFFF...FFFF: the high word is all ones
    CHECK_EQ(imulhi(0x40000000, 4), 1);

    TEST_SECTION("helpers");
    CHECK(bit_is_pow2(64)); CHECK(!bit_is_pow2(65)); CHECK(!bit_is_pow2(0));
    CHECK_EQ((int)bit_next_pow2(33), 64); CHECK_EQ((int)bit_next_pow2(64), 64); CHECK_EQ((int)bit_next_pow2(1), 1);
    CHECK_EQ((int)bit_log2(1024), 10); CHECK_EQ((int)bit_log2(1), 0);
    CHECK_EQ((int)align_up(13, 8), 16); CHECK_EQ((int)align_up(16, 8), 16); CHECK_EQ((int)align_down(13, 8), 8);
    unsigned int flags = 0;
    BIT_SET(flags, 5); CHECK_EQ((int)BIT_TEST(flags, 5), 1); CHECK_EQ((int)flags, 32);
    BIT_CLR(flags, 5); CHECK_EQ((int)flags, 0);

    TEST_SECTION("the macros and the functions agree");
    unsigned int (*count[3])(unsigned int) = { bit_clz, bit_ctz, bit_popcount };
    unsigned int samples[6];
    samples[0] = 0; samples[1] = 1; samples[2] = 0x80000000u; samples[3] = 0x00F0F000u; samples[4] = 0xFFFFFFFFu; samples[5] = 12345u;
    int same = 1;
    for (int i = 0; i < 6; i++)
    {
        unsigned int x = samples[i];
        if (bit_clz(x) != count[0](x) || bit_ctz(x) != count[1](x) || bit_popcount(x) != count[2](x))
            same = 0;
        if (bit_bswap(x) != (bit_bswap)(x) || umulhi(x, 0x9E3779B9u) != (umulhi)(x, 0x9E3779B9u))
            same = 0;
        if (imulhi((int)x, -7) != (imulhi)((int)x, -7))
            same = 0;
        for (int n = 0; n < 40; n += 7)
            if (bit_rotl(x, n) != (bit_rotl)(x, n) || bit_rotr(x, n) != (bit_rotr)(x, n))
                same = 0;
    }
    CHECK(same);
    CHECK_EQ((int)bit_popcount(-1), 32);                  // an int argument, converted
    CHECK_EQ((int)bit_clz((unsigned char)1), 31);
    return test_summary();
}
