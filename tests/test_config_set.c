// The same settings, changed for the whole build by definitions on the compiler's command line
// (tests/expected/test_config_set.flags): the library is compiled with them too, so its tables shrink.
#include "ceres/test.h"
#include "ceres/config.h"
#include "ceres/timer.h"
#include "ceres/fs.h"
#include "ceres/heap.h"
#include "ceres/sys.h"
#include "stdlib.h"

static void nothing(void) {}
static void task(void* ctx) {}

int main(void)
{
    TEST_SECTION("the settings the build chose");
    CHECK_EQ(CERES_ATEXIT_SLOTS, 4);
    CHECK_EQ(CERES_HEAP_STACK_RESERVE, 4096);
    CHECK_EQ(FS_MAX_OPEN, 3);
    CHECK_EQ(TIMER_MAX_TASKS, 2);

    TEST_SECTION("and the library follows them");
    int accepted = 0;
    for (int i = 0; i < 10; i++)
        if (atexit(nothing) == 0)
            accepted++;
    CHECK_EQ(accepted, 4);

    int tasks = 0;
    for (int i = 0; i < 10; i++)
        if (timer_after(1000000, task, 0) >= 0)
            tasks++;
    CHECK_EQ(tasks, 2);

    struct heap_stats s;
    heap_stats(&s);
    unsigned int sp = sys_sp();
    CHECK(s.limit > sp - 4096u - 2048u && s.limit < sp - 4096u + 2048u);   // the reserve is the one asked for, give or take this call's frame
    return test_summary();
}
