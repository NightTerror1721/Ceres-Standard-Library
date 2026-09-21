// 64-bit counts in two words: exact arithmetic, and a division that is checked by multiplying back.
#include "ceres/test.h"
#include "ceres/ns64.h"

// hi first, as one writes a number
static struct ns64 n(unsigned int hi, unsigned int lo)
{
    return ns64_make(lo, hi);
}

#define IS(a, h, l) \
    do { struct ns64 __v = (a); __t_total++; \
        if (__v.hi != (h) || __v.lo != (l)) { __t_failed++; \
            printf("FAIL %s:%d: %s is %x:%x, not %x:%x\n", __FILE__, __LINE__, #a, __v.hi, __v.lo, (unsigned int)(h), (unsigned int)(l)); } } while (0)

static unsigned int seed = 12345u;

static unsigned int rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

int main(void)
{
    TEST_SECTION("make and compare");
    IS(ns64_make(7u, 9u), 9u, 7u);
    IS(ns64_from_u32(0xFFFFFFFFu), 0u, 0xFFFFFFFFu);
    CHECK(ns64_is_zero(n(0u, 0u)));
    CHECK(!ns64_is_zero(n(0u, 1u)));
    CHECK(!ns64_is_zero(n(1u, 0u)));
    CHECK_EQ(ns64_cmp(n(0u, 5u), n(0u, 5u)), 0);
    CHECK_EQ(ns64_cmp(n(0u, 4u), n(0u, 5u)), -1);
    CHECK_EQ(ns64_cmp(n(1u, 0u), n(0u, 0xFFFFFFFFu)), 1);   // the high word decides
    CHECK_EQ(ns64_cmp(n(0u, 0x80000000u), n(0u, 1u)), 1);   // unsigned: a set top bit is large, not negative
    CHECK_EQ(ns64_cmp(n(0x80000000u, 0u), n(1u, 0u)), 1);

    TEST_SECTION("add carries");
    IS(ns64_add(n(0u, 0xFFFFFFFFu), n(0u, 1u)), 1u, 0u);
    IS(ns64_add(n(1u, 0xFFFFFFFFu), n(1u, 0xFFFFFFFFu)), 3u, 0xFFFFFFFEu);
    IS(ns64_add(n(0xFFFFFFFFu, 0xFFFFFFFFu), n(0u, 1u)), 0u, 0u);   // wraps at 2^64
    IS(ns64_add(n(2u, 3u), n(0u, 0u)), 2u, 3u);

    TEST_SECTION("sub borrows");
    IS(ns64_sub(n(1u, 0u), n(0u, 1u)), 0u, 0xFFFFFFFFu);
    IS(ns64_sub(n(3u, 0xFFFFFFFEu), n(1u, 0xFFFFFFFFu)), 1u, 0xFFFFFFFFu);
    IS(ns64_sub(n(0u, 0u), n(0u, 1u)), 0xFFFFFFFFu, 0xFFFFFFFFu);   // below zero wraps
    IS(ns64_sub(n(5u, 5u), n(5u, 5u)), 0u, 0u);
    // A difference is right when the later reading has wrapped past the earlier one
    IS(ns64_sub(n(0u, 9u), n(0xFFFFFFFFu, 0xFFFFFFF0u)), 0u, 0x19u);

    TEST_SECTION("multiply");
    IS(ns64_mul_u32(n(0u, 0xFFFFFFFFu), 0xFFFFFFFFu), 0xFFFFFFFEu, 1u);
    IS(ns64_mul_u32(n(1u, 5u), 3u), 3u, 15u);
    IS(ns64_mul_u32(n(0x12345678u, 0x9ABCDEF0u), 1000u), 0x1C71C71Cu, 0x71C6D980u);   // modulo 2^64
    IS(ns64_mul_u32(n(9u, 9u), 0u), 0u, 0u);
    IS(ns64_mul_u32(n(0u, 0x10000u), 0x10000u), 1u, 0u);

    TEST_SECTION("from a span");
    IS(ns64_from_us(1500u), 0u, 1500000u);
    IS(ns64_from_ms(1500u), 0u, 1500000000u);
    IS(ns64_from_sec(2u), 0u, 2000000000u);
    IS(ns64_from_ms(0xFFFFFFFFu), 0xF423Fu, 0xFFF0BDC0u);
    IS(ns64_from_us(0xFFFFFFFFu), 0x3E7u, 0xFFFFFC18u);
    IS(ns64_from_sec(0xFFFFFFFFu), 0x3B9AC9FFu, 0xC4653600u);

    TEST_SECTION("to a span rounds down");
    CHECK_EQ((int)ns64_to_us(n(0u, 1999u)), 1);
    CHECK_EQ((int)ns64_to_us(n(0u, 999u)), 0);
    CHECK_EQ((int)ns64_to_ms(ns64_from_ms(1234u)), 1234);
    CHECK_EQ((int)ns64_to_ms(ns64_add(ns64_from_ms(7u), ns64_from_u32(999999u))), 7);
    CHECK_EQ((int)ns64_to_sec(ns64_from_sec(90u)), 90);
    CHECK_EQ((int)ns64_to_us(ns64_from_us(0xFFFFFFFFu)), -1);      // the largest that fits, exactly
    CHECK_EQ((int)ns64_to_ms(ns64_from_ms(0xFFFFFFFFu)), -1);
    CHECK_EQ((int)ns64_to_sec(ns64_from_sec(0xFFFFFFFFu)), -1);

    TEST_SECTION("to a span saturates");
    CHECK_EQ((int)ns64_to_us(n(1000u, 0u)), -1);                  // 2^32 microseconds: one too many
    CHECK_EQ((int)ns64_to_us(n(999u, 0u)), (int)4290672328u);     // and just under it, the real count
    CHECK_EQ((int)ns64_to_ms(n(0xFFFFFFFFu, 0xFFFFFFFFu)), -1);
    CHECK_EQ((int)ns64_to_sec(n(0xFFFFFFFFu, 0xFFFFFFFFu)), -1);

    TEST_SECTION("division");
    unsigned int rem = 123u;
    IS(ns64_div_u32(n(0u, 1000u), 10u, &rem), 0u, 100u);
    CHECK_EQ((int)rem, 0);
    IS(ns64_div_u32(n(0u, 1005u), 10u, &rem), 0u, 100u);
    CHECK_EQ((int)rem, 5);
    IS(ns64_div_u32(n(0x12345678u, 0x9ABCDEF0u), 1000u, &rem), 0x4A90Bu, 0xE587DE6Eu);
    CHECK_EQ((int)rem, 320);
    IS(ns64_div_u32(n(0x12345678u, 0x9ABCDEF0u), 1000000u, &rem), 0x131u, 0x6B7E5807u);   // more than 16 bits: the bit-by-bit path
    CHECK_EQ((int)rem, 790320);
    IS(ns64_div_u32(n(0xE8u, 0xD4A51000u), 1000000007u, &rem), 0u, 999u);
    CHECK_EQ((int)rem, 999993007);
    IS(ns64_div_u32(n(0xFFFFFFFFu, 0xFFFFFFFFu), 0xFFFFFFFFu, &rem), 1u, 1u);                // 2^64-1 = (2^32+1)(2^32-1)
    CHECK_EQ((int)rem, 0);
    IS(ns64_div_u32(n(0u, 7u), 1u, 0), 0u, 7u);                                              // no remainder asked for
    IS(ns64_div_u32(n(1u, 2u), 0u, &rem), 0xFFFFFFFFu, 0xFFFFFFFFu);                         // by zero: the largest count
    CHECK_EQ((int)rem, 0);

    TEST_SECTION("division is undone by multiplying back");
    static const unsigned int divisors[] = { 1u, 2u, 7u, 999u, 1000u, 65535u, 65536u, 65537u, 1000000u, 999999937u, 0x80000000u, 0x80000001u, 0xFFFFFFFFu };
    int bad = 0;
    for (int round = 0; round < 40; round++)
    {
        struct ns64 a = n(rnd() >> (round % 5 * 6), rnd());
        for (int i = 0; i < (int)(sizeof(divisors) / sizeof(divisors[0])); i++)
        {
            unsigned int d = divisors[i];
            unsigned int r;
            struct ns64 q = ns64_div_u32(a, d, &r);
            struct ns64 back = ns64_add(ns64_mul_u32(q, d), ns64_from_u32(r));
            if (r >= d || ns64_cmp(back, a) != 0)
                bad++;
        }
    }
    CHECK_EQ(bad, 0);

    TEST_SECTION("to float");
    CHECK_NEAR(ns64_to_float(n(0u, 1000000u)), 1000000.0f, 0.5f);
    CHECK_NEAR(ns64_to_float(n(1u, 0u)), 4294967296.0f, 512.0f);
    CHECK_NEAR(ns64_to_float(n(0u, 0u)), 0.0f, 0.0f);
    CHECK_NEAR(ns64_to_float(n(0x80000000u, 0u)) / 9223372036854775808.0f, 1.0f, 0.0001f);

    return test_summary();
}
