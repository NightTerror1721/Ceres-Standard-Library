// USE: fault
// SIGFPE from a division by zero, through the fault module: the handler runs, and the program carries on after
// the division with every register as it was. Integer and float divisions both count. SIG_DFL puts the machine
// back to ignoring a division by zero, SIG_IGN swallows it.
#include "ceres/test.h"
#include "ceres/sys.h"
#include "signal.h"
#include "errno.h"

static volatile int divisions;
static volatile int last_signal;

static void on_fpe(int sig)
{
    last_signal = sig;
    divisions = divisions + 1;
}

// Values that live in registers across the division, so a handler that clobbered one would show.
static int mix(int a, int b, int c, int d, int zero)
{
    int p = a * 3 + 1;
    int q = b * 5 + 2;
    int r = c * 7 + 3;
    int s = d * 11 + 4;
    float f = (float)a * 1.5f;
    float g = (float)b * 2.5f;
    volatile int sink = a / zero;                        // the division by zero
    sink = sink + 0;
    return p + q + r + s + (int)(f + g);
}

int main(void)
{
    TEST_SECTION("before the vectors are bound");
    errno = 0;
    CHECK(signal(SIGFPE, on_fpe) == SIG_ERR);
    CHECK_EQ(errno, ENOSYS);
    CHECK_EQ((int)sys_features(), 0);                    // and nothing was switched on

    TEST_SECTION("a handler for SIGFPE");
    sys_install_fault_handlers(0);
    CHECK(signal(SIGFPE, on_fpe) == SIG_DFL);
    CHECK_EQ((int)(sys_features() & SYS_FEATURE_DIV_FAULT), SYS_FEATURE_DIV_FAULT);

    volatile int a = 10;
    volatile int zero = 0;
    volatile int sink = a / zero;
    sink = sink + 0;
    CHECK_EQ(divisions, 1);                              // and the program is still here
    CHECK_EQ(last_signal, SIGFPE);

    TEST_SECTION("the registers come back as they were");
    int before = 1 * 3 + 1 + 2 * 5 + 2 + 3 * 7 + 3 + 4 * 11 + 4 + (int)(1.0f * 1.5f + 2.0f * 2.5f);
    CHECK_EQ(mix(1, 2, 3, 4, zero), before);
    CHECK_EQ(divisions, 2);
    CHECK_EQ(mix(1, 2, 3, 4, zero), before);
    CHECK_EQ(divisions, 3);

    TEST_SECTION("a float division counts too");
    volatile float one = 1.0f;
    volatile float nothing = 0.0f;
    volatile float ratio = one / nothing;
    ratio = ratio + 0.0f;
    CHECK_EQ(divisions, 4);
    volatile int rem = a % zero;                         // and so does a remainder (volatile: -O1 drops an unused one)
    rem = rem + 0;
    CHECK_EQ(divisions, 5);

    TEST_SECTION("raise runs the same handler");
    CHECK_EQ(raise(SIGFPE), 0);
    CHECK_EQ(divisions, 6);

    TEST_SECTION("SIG_IGN swallows it");
    CHECK(signal(SIGFPE, SIG_IGN) == on_fpe);
    sink = a / zero;
    sink = sink + 0;
    CHECK_EQ(divisions, 6);

    TEST_SECTION("SIG_DFL puts the machine back to carrying on silently");
    CHECK(signal(SIGFPE, SIG_DFL) == SIG_IGN);
    CHECK_EQ((int)(sys_features() & SYS_FEATURE_DIV_FAULT), 0);
    sink = a / zero;
    sink = sink + 0;
    CHECK_EQ(divisions, 6);
    return test_summary();
}
