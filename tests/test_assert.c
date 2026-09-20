// assert(): silent while true, one line and a stop when false, and NDEBUG removes it.
#include "ceres/test.h"
#include "assert.h"

static int evaluations;
static int touch(int v) { evaluations++; return v; }

// A second inclusion after NDEBUG must turn assert into a no-op, and a third must bring it back.
#define NDEBUG
#include "assert.h"
static void disabled(void)
{
    assert(0);                       // nothing happens: the expression is not even evaluated
    assert(touch(0));
}
#undef NDEBUG
#include "assert.h"

int main(void)
{
    TEST_SECTION("true assertions are silent");
    assert(1);
    assert(touch(5) == 5);
    CHECK_EQ(evaluations, 1);

    TEST_SECTION("NDEBUG");
    disabled();
    CHECK_EQ(evaluations, 1);        // touch() was never called

    TEST_SECTION("a failed assertion");
    int verdict = test_summary();
    assert(touch(0) == 1);           // prints "<file>:<line>: assertion 'touch(0) == 1' failed" and stops
    putstr("NOT REACHED\n");
    return verdict;
}
