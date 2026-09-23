// Real 64-bit integers in the library: the int64_t/uint64_t types and their limits and constant
// macros (stdint.h), the 64-bit printf/scanf conversions (PRI*/SCN* in inttypes.h), and the 64-bit
// strtox and arithmetic (strtoll/strtoull/atoll, llabs/lldiv, imaxabs/imaxdiv).
#include "ceres/test.h"
#include "stdint.h"
#include "inttypes.h"
#include "stdlib.h"
#include "limits.h"
#include "string.h"

// The types really are eight bytes, and signed/unsigned as their names say. _Static_assert is a
// compile-time check: a wrong size is a build failure, not a printed FAIL.
_Static_assert(sizeof(int64_t) == 8, "int64_t is 8 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t is 8 bytes");
_Static_assert(sizeof(int_least64_t) == 8, "int_least64_t is 8 bytes");
_Static_assert(sizeof(int_fast64_t) == 8, "int_fast64_t is 8 bytes");
_Static_assert(sizeof(intmax_t) == 8, "intmax_t is 8 bytes");
_Static_assert(sizeof(uintmax_t) == 8, "uintmax_t is 8 bytes");

int main(void)
{
    TEST_SECTION("the limits");
    CHECK_EQ64(INT64_MAX, 9223372036854775807LL);
    CHECK_EQ64(INT64_MIN, (-9223372036854775807LL - 1));
    CHECK_EQ64(LLONG_MAX, 9223372036854775807LL);
    CHECK_EQ64(LLONG_MIN, (-9223372036854775807LL - 1));
    CHECK((uint64_t)UINT64_MAX == 18446744073709551615ULL);
    CHECK((uint64_t)ULLONG_MAX == 18446744073709551615ULL);
    CHECK_EQ64((long long)((uint64_t)INT64_MAX + 1u), INT64_MIN); // the wrapped bit pattern is INT64_MIN
    CHECK_EQ64(INT_LEAST64_MAX, INT64_MAX);
    CHECK_EQ64(INT_FAST64_MIN, INT64_MIN);
    CHECK_EQ64(INTMAX_MAX, INT64_MAX);

    TEST_SECTION("the constant macros give a 64-bit value");
    CHECK_EQ64(INT64_C(1234567890123), 1234567890123LL);
    CHECK((uint64_t)UINT64_C(18446744073709551615) == 18446744073709551615ULL);
    CHECK_EQ64(INTMAX_C(42), 42LL);
    CHECK((uint64_t)UINTMAX_C(42) == 42ULL);

    TEST_SECTION("arithmetic on the real type");
    int64_t a = 0x0000000100000002LL;                           // hi 1, lo 2 - does not fit 32 bits
    CHECK_EQ64(a + a, 0x0000000200000004LL);
    CHECK_EQ64(a * 3, 0x0000000300000006LL);
    CHECK_EQ64(-a, -0x0000000100000002LL);
    CHECK_EQ64(a << 32, 0x0000000200000000LL);                  // the high word shifts into the low one
    CHECK_EQ64((int64_t)(a >> 32), 1);                          // the high word really is there
    CHECK((uint64_t)a > 0xFFFFFFFFULL);                         // the low word alone would not say so
    CHECK_EQ64((int64_t)(float)0x100000000LL, 0x100000000LL);   // float <-> 64-bit round trip

    TEST_SECTION("printf reads a 64-bit argument as two words");
    char buf[96];
    snprintf(buf, sizeof buf, "%lld", a);
    CHECK_STR(buf, "4294967298");
    snprintf(buf, sizeof buf, "%lld", -a);
    CHECK_STR(buf, "-4294967298");
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)0xFFFFFFFFFFFFFFFFULL);
    CHECK_STR(buf, "18446744073709551615");
    snprintf(buf, sizeof buf, "%llx", 0x0000000ABCDEF012LL);
    CHECK_STR(buf, "abcdef012");
    snprintf(buf, sizeof buf, "%llX", 0x0000000ABCDEF012LL);
    CHECK_STR(buf, "ABCDEF012");
    snprintf(buf, sizeof buf, "%llo", 8LL);
    CHECK_STR(buf, "10");
    snprintf(buf, sizeof buf, "%+lld %lld %lld", 5LL, -5LL, (long long)INT64_MIN);
    CHECK_STR(buf, "+5 -5 -9223372036854775808");
    snprintf(buf, sizeof buf, "%020lld", 42LL);
    CHECK_STR(buf, "00000000000000000042");
    snprintf(buf, sizeof buf, "%lld %d %llu %d", a, 7, (unsigned long long)9ULL, 8);
    CHECK_STR(buf, "4294967298 7 9 8");                         // mixed widths in one call

    TEST_SECTION("the inttypes.h PRI macros name the width");
    snprintf(buf, sizeof buf, "%" PRId64, a);
    CHECK_STR(buf, "4294967298");
    snprintf(buf, sizeof buf, "%" PRIu64, (uint64_t)18446744073709551615ULL);
    CHECK_STR(buf, "18446744073709551615");
    snprintf(buf, sizeof buf, "%" PRIx64, 0x0000000100000002LL);
    CHECK_STR(buf, "100000002");
    snprintf(buf, sizeof buf, "%" PRIdMAX, (intmax_t)-1);
    CHECK_STR(buf, "-1");

    TEST_SECTION("scanf writes a 64-bit value back as two words");
    int64_t s = 0;
    unsigned long long u = 0;
    int n = sscanf("987654321098765", "%lld", &s);
    CHECK_EQ(n, 1);
    CHECK_EQ64(s, 987654321098765LL);
    n = sscanf("-9223372036854775808", "%lld", &s);
    CHECK_EQ(n, 1);
    CHECK_EQ64(s, INT64_MIN);
    n = sscanf("18446744073709551615", "%llu", &u);
    CHECK_EQ(n, 1);
    CHECK(u == 18446744073709551615ULL);
    n = sscanf("deadbeef", "%llx", &u);
    CHECK_EQ(n, 1);
    CHECK(u == 0xDEADBEEFULL);
    int scratch = 0;
    n = sscanf("7 4294967298 9", "%d %lld %d", &scratch, &s, &u); // `n` stays the return value alone
    CHECK_EQ(n, 3);

    TEST_SECTION("strtoll, strtoull and atoll");
    char* end;
    CHECK_EQ64(strtoll("1234567890123", &end, 10), 1234567890123LL);
    CHECK_EQ64(strtoll("-9223372036854775808", 0, 10), INT64_MIN);
    CHECK_EQ64(strtoll("0x100000002", 0, 0), 0x0000000100000002LL);
    CHECK_EQ64(strtoll("777", 0, 8), 511LL);
    CHECK(strtoull("18446744073709551615", 0, 10) == 18446744073709551615ULL);
    CHECK(strtoull("-1", 0, 10) == 18446744073709551615ULL);    // "-1" wraps, as in C
    CHECK_EQ64(strtoll("  -42xyz", &end, 10), -42LL);
    CHECK_STR(end, "xyz");
    CHECK_EQ64(atoll("-4294967298"), -4294967298LL);
    CHECK_EQ64(atoll("0"), 0LL);

    TEST_SECTION("strtoll reports overflow at the 64-bit limit");
    CHECK_EQ64(strtoll("9223372036854775808", 0, 10), INT64_MAX);   // one past LLONG_MAX
    CHECK_EQ64(strtoll("-9223372036854775809", 0, 10), INT64_MIN);  // one past LLONG_MIN
    CHECK(strtoull("18446744073709551616", 0, 10) == 18446744073709551615ULL);

    TEST_SECTION("llabs, lldiv, imaxabs and imaxdiv");
    CHECK_EQ64(llabs(-0x0000000100000002LL), 0x0000000100000002LL);
    CHECK_EQ64(llabs(0x0000000100000002LL), 0x0000000100000002LL);
    lldiv_t d = lldiv(0x0000000100000007LL, 5);                 // 4294967303 = 858993460*5 + 3
    CHECK_EQ64(d.quot, 858993460LL);
    CHECK_EQ64(d.rem, 3LL);
    d = lldiv(-0x0000000100000002LL, 3);                        // -4294967298 = 3 * -1431655766 + 0
    CHECK_EQ64(d.quot, -1431655766LL);
    CHECK_EQ64(d.rem, 0LL);
    d = lldiv(-0x0000000100000003LL, 3);                        // -4294967299 = 3 * -1431655766 + -1
    CHECK_EQ64(d.quot, -1431655766LL);
    CHECK_EQ64(d.rem, -1LL);
    CHECK_EQ64(imaxabs((intmax_t)-7), 7LL);
    imaxdiv_t id = imaxdiv((intmax_t)0x0000000100000007LL, (intmax_t)5);
    CHECK_EQ64(id.quot, 858993460LL);
    CHECK_EQ64(id.rem, 3LL);

    return test_summary();
}
