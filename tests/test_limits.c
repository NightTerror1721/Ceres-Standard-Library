// Regression for <limits.h> and <stddef.h>: UINT_MAX and ULONG_MAX were negative (no 'u' suffix), so
// `UINT_MAX > 0` was false.
#include "ceres/test.h"
#include "limits.h"
#include "stddef.h"

struct Padded { char tag; int value; char flag; };

int main(void)
{
    TEST_SECTION("unsigned maxima");
    CHECK(UINT_MAX > 0);
    CHECK(ULONG_MAX > 0);
    CHECK(SIZE_MAX > 0);
    CHECK(UINT_MAX == 0xFFFFFFFFu);
    CHECK(UINT_MAX + 1u == 0u);
    CHECK(USHRT_MAX == 65535);
    CHECK(UCHAR_MAX == 255);

    TEST_SECTION("signed extremes");
    CHECK(INT_MIN < 0);
    CHECK(INT_MAX > 0);
    CHECK(INT_MIN == -INT_MAX - 1);
    CHECK((INT_MAX + 1) < 0);            // a signed overflow wraps, as on the hardware
    CHECK(LONG_MIN == INT_MIN);
    CHECK(LONG_MAX == INT_MAX);
    CHECK(SHRT_MIN == -32768);
    CHECK(SHRT_MAX == 32767);
    CHECK(SCHAR_MIN == -128);
    CHECK(CHAR_MIN == SCHAR_MIN);        // plain char is signed
    CHECK(CHAR_BIT == 8);

    TEST_SECTION("widths");
    CHECK_EQ((int)sizeof(int), 4);
    CHECK_EQ((int)sizeof(long), 4);
    CHECK_EQ((int)sizeof(size_t), 4);
    CHECK_EQ((int)sizeof(void*), 4);
    char c = (char)200;
    CHECK(c < 0);

    TEST_SECTION("stddef");
    CHECK(NULL == 0);
    CHECK_EQ((int)offsetof(struct Padded, tag), 0);
    CHECK_EQ((int)offsetof(struct Padded, value), 4);
    CHECK_EQ((int)offsetof(struct Padded, flag), 8);
    CHECK_EQ((int)sizeof(struct Padded), 12);
    return test_summary();
}
