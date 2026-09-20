// <float.h>, <stdint.h>, <inttypes.h> and <errno.h>: the constants mean what they say.
#include "ceres/test.h"
#include "float.h"
#include "stdint.h"
#include "inttypes.h"
#include "errno.h"
#include "math.h"

static int checked_calls;
static uint8_t widen(uint8_t v) { checked_calls++; return v; }

int main(void)
{
    TEST_SECTION("float.h");
    CHECK_EQ(FLT_RADIX, 2);
    CHECK_EQ(FLT_MANT_DIG, 24);
    CHECK_EQ(FLT_DIG, 6);
    CHECK_EQ(FLT_DECIMAL_DIG, 9);
    CHECK(DBL_MAX == FLT_MAX);
    CHECK(LDBL_EPSILON == FLT_EPSILON);
    volatile float one = 1.0f;                          // volatile: keep the compiler from folding
    CHECK(one + FLT_EPSILON > one);                     // epsilon is the gap above 1.0
    CHECK(one + FLT_EPSILON / 2.0f == one);             // half of it is rounded away
    CHECK(FLT_MIN > 0.0f);
    CHECK(FLT_TRUE_MIN > 0.0f);
    volatile float tiny = FLT_TRUE_MIN;
    CHECK(tiny / 2.0f == 0.0f);                         // nothing is smaller than the smallest subnormal
    volatile float big = FLT_MAX;
    CHECK(isinf(big * 2.0f));                           // overflow gives infinity
    CHECK(FLT_MAX_10_EXP == 38 && FLT_MIN_10_EXP == -37);
    CHECK(FLT_HAS_SUBNORM == 1);

    TEST_SECTION("stdint.h widths");
    CHECK_EQ((int)sizeof(int8_t), 1);
    CHECK_EQ((int)sizeof(uint8_t), 1);
    CHECK_EQ((int)sizeof(int16_t), 2);
    CHECK_EQ((int)sizeof(uint16_t), 2);
    CHECK_EQ((int)sizeof(int32_t), 4);
    CHECK_EQ((int)sizeof(uint32_t), 4);
    CHECK_EQ((int)sizeof(intptr_t), 4);
    CHECK_EQ((int)sizeof(uintmax_t), 4);
    CHECK_EQ((int)sizeof(int_fast8_t), 4);              // fast types are register-sized
    CHECK_EQ((int)sizeof(int_least8_t), 1);

    TEST_SECTION("stdint.h limits");
    CHECK(INT8_MIN == -128 && INT8_MAX == 127 && UINT8_MAX == 255);
    CHECK(INT16_MIN == -32768 && INT16_MAX == 32767 && UINT16_MAX == 65535);
    CHECK(INT32_MIN < 0 && INT32_MAX > 0);
    CHECK(INT32_MIN == -INT32_MAX - 1);
    CHECK(UINT32_MAX > 0);                              // the trap in <limits.h>: it needs its `u`
    CHECK(UINT32_MAX + 1u == 0u);
    CHECK(UINTPTR_MAX == UINT32_MAX && INTPTR_MAX == INT32_MAX);
    CHECK(SIZE_MAX == UINT32_MAX);
    int8_t s8 = (int8_t)200;
    uint8_t u8 = (uint8_t)200;
    CHECK(s8 == -56);
    CHECK(u8 == 200);
    CHECK((uint8_t)(u8 + 100) == 44);                   // wraps at 8 bits
    CHECK((int16_t)40000 == -25536);

    TEST_SECTION("stdint.h constants");
    uint32_t big32 = UINT32_C(4000000000);
    CHECK(big32 > 3999999999u && big32 < 4000000001u);
    CHECK(UINT8_C(255) == 255u);
    CHECK(INT16_C(-5) == -5);
    CHECK_EQ((int)widen(UINT8_C(7)), 7);
    CHECK_EQ(checked_calls, 1);

    TEST_SECTION("inttypes.h");
    CHECK_STR(PRId32, "d");
    CHECK_STR(PRIu32, "u");
    CHECK_STR(PRIx32, "x");
    CHECK_STR(PRIX32, "X");
    CHECK_STR(PRIo32, "o");
    CHECK_STR(PRIuPTR, "u");
    CHECK_STR(SCNu32, "u");

    TEST_SECTION("errno.h");
    CHECK_EQ(errno, 0);                                 // starts clear
    errno = ERANGE;
    CHECK_EQ(errno, 34);
    errno = 0;
    CHECK(EDOM == 33 && ERANGE == 34 && EINVAL == 22 && ENOMEM == 12 && ENOENT == 2);
    CHECK_STR(strerror(0), "Success");
    CHECK_STR(strerror(ENOENT), "No such file or directory");
    CHECK_STR(strerror(ENOMEM), "Out of memory");
    CHECK_STR(strerror(EDOM), "Domain error");
    CHECK_STR(strerror(ERANGE), "Result out of range");
    CHECK_STR(strerror(EINVAL), "Invalid argument");
    CHECK_STR(strerror(-1), "Unknown error");
    CHECK_STR(strerror(9999), "Unknown error");
    return test_summary();
}
