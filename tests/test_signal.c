// signal() and raise(): the six signal numbers, handlers, SIG_IGN and SIG_DFL, and what is refused. The
// hardware signals need the fault module (test_signal_hw); here they are refused.
#include "ceres/test.h"
#include "signal.h"
#include "errno.h"

static volatile int last;
static volatile int count;

static void handler(int sig)
{
    last = sig;
    count = count + 1;
}

static void other(int sig)
{
    count = count + 100;
}

int main(void)
{
    TEST_SECTION("numbers");
    CHECK_EQ(SIGINT, 2);
    CHECK_EQ(SIGILL, 4);
    CHECK_EQ(SIGABRT, 6);
    CHECK_EQ(SIGFPE, 8);
    CHECK_EQ(SIGSEGV, 11);
    CHECK_EQ(SIGTERM, 15);
    CHECK(SIG_DFL != SIG_IGN);
    CHECK(SIG_ERR != SIG_DFL);

    TEST_SECTION("a handler runs on raise, and stays installed");
    CHECK(signal(SIGTERM, handler) == SIG_DFL);          // nothing was installed before
    CHECK_EQ(raise(SIGTERM), 0);
    CHECK_EQ(last, SIGTERM);
    CHECK_EQ(count, 1);
    CHECK_EQ(raise(SIGTERM), 0);
    CHECK_EQ(count, 2);

    TEST_SECTION("signal returns the previous handler");
    CHECK(signal(SIGTERM, other) == handler);
    CHECK_EQ(raise(SIGTERM), 0);
    CHECK_EQ(count, 102);
    CHECK(signal(SIGTERM, SIG_IGN) == other);
    CHECK(signal(SIGTERM, SIG_DFL) == SIG_IGN);

    TEST_SECTION("SIG_IGN swallows the signal");
    CHECK(signal(SIGINT, SIG_IGN) == SIG_DFL);
    count = 0;
    CHECK_EQ(raise(SIGINT), 0);
    CHECK_EQ(count, 0);
    CHECK(signal(SIGINT, handler) == SIG_IGN);
    CHECK_EQ(raise(SIGINT), 0);
    CHECK_EQ(last, SIGINT);
    CHECK(signal(SIGINT, SIG_DFL) == handler);

    TEST_SECTION("what is refused");
    errno = 0;
    CHECK(signal(99, handler) == SIG_ERR);
    CHECK_EQ(errno, EINVAL);
    errno = 0;
    CHECK(signal(0, handler) == SIG_ERR);
    CHECK_EQ(errno, EINVAL);
    errno = 0;
    CHECK(signal(SIGINT, SIG_ERR) == SIG_ERR);
    CHECK_EQ(errno, EINVAL);
    errno = 0;
    CHECK_EQ(raise(0), -1);
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(raise(64), -1);

    TEST_SECTION("the hardware signals want the fault module");
    errno = 0;
    CHECK(signal(SIGSEGV, handler) == SIG_ERR);
    CHECK_EQ(errno, ENOSYS);
    errno = 0;
    CHECK(signal(SIGFPE, handler) == SIG_ERR);
    CHECK_EQ(errno, ENOSYS);
    CHECK(signal(SIGILL, SIG_IGN) == SIG_DFL);           // ignoring one needs nothing delivered
    CHECK(signal(SIGILL, SIG_DFL) == SIG_IGN);

    TEST_SECTION("sig_atomic_t");
    sig_atomic_t flag = 0;
    flag = 5;
    CHECK_EQ(flag, 5);
    return test_summary();
}
