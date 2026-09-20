// Every public header, in ONE translation unit: they must not redefine each other's names, and
// each must find what it needs on its own (a header that only compiles after another one is a bug).
// tools/runtests.ps1 also compiles each header ALONE (-Headers) - that is the half of the check
// this file cannot do.
#include "ceres.h"
#include "ctype.h"
#include "float.h"
#include "interrupts.h"
#include "limits.h"
#include "math.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "strings.h"
#include "ceres/bits.h"
#include "ceres/display.h"
#include "ceres/gamepad.h"
#include "ceres/heap.h"
#include "ceres/irq.h"
#include "ceres/keyboard.h"
#include "ceres/mouse.h"
#include "ceres/sys.h"
#include "ceres/terminal.h"
#include "ceres/test.h"

// The names a user is entitled to. math.h used to #define PI, LN2, SQRT2... which made these
// declarations a syntax error.
enum { PI, E, LN2, SQRT2, HALF_PI };

static int sum_ints(int count, ...)
{
    va_list ap;
    int total = 0;
    va_start(ap, count);
    for (int i = 0; i < count; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

int main(void)
{
    TEST_SECTION("names stay the user's");
    CHECK_EQ(PI, 0);
    CHECK_EQ(SQRT2, 3);

    TEST_SECTION("constants from several headers agree");
    CHECK_EQ((int)(M_PI * 1000.0f), 3141);
    CHECK_EQ((int)(M_SQRT2 * 1000.0f), 1414);
    CHECK_EQ(TERM_INPUT_READY, 1);
    CHECK_EQ(IRQ_TIMER, 16);
    CHECK_EQ(IRQ_COUNT, 64);
    CHECK_EQ((int)(TERMINAL_BASE == 0xFF000000u), 1);
    CHECK_EQ((int)RGB(1, 2, 3), 0x010203);
    CHECK_EQ(GP_BTN_DPAD_UP, 0x0800);

    TEST_SECTION("stdarg");
    CHECK_EQ(sum_ints(4, 10, 20, 30, 40), 100);
    CHECK_EQ(sum_ints(0), 0);

    TEST_SECTION("float classification");
    CHECK(isnan(NAN));
    CHECK(!isnan(1.5f));
    CHECK(isinf(INFINITY));
    CHECK(isinf(-INFINITY));
    CHECK(!isinf(NAN));
    CHECK(signbit(-2.0f));
    CHECK(!signbit(2.0f));
    CHECK(float_bits(1.0f) == 0x3F800000u);
    CHECK_NEAR(float_from_bits(0x40490FDBu), M_PI, 0.00001f);
    CHECK(FLT_EPSILON > 0.0f);
    return test_summary();
}
