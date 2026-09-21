// The compile-time settings at their defaults (test_config_set changes them).
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
    TEST_SECTION("the defaults");
    CHECK_EQ(CERES_ATEXIT_SLOTS, 32);
    CHECK_EQ(CERES_HEAP_STACK_RESERVE, 16384);
    CHECK_EQ(FS_MAX_OPEN, 8);
    CHECK_EQ(TIMER_MAX_TASKS, 8);

    TEST_SECTION("and the library uses them");
    int accepted = 0;
    for (int i = 0; i < 40; i++)
        if (atexit(nothing) == 0)
            accepted++;
    CHECK_EQ(accepted, 32);                             // atexit holds CERES_ATEXIT_SLOTS

    int tasks = 0;
    for (int i = 0; i < 20; i++)
        if (timer_after(1000000, task, 0) >= 0)
            tasks++;
    CHECK_EQ(tasks, 8);                                 // and the timer table TIMER_MAX_TASKS

    struct heap_stats s;
    heap_stats(&s);
    unsigned int sp = sys_sp();
    CHECK(s.limit > sp - 16384u - 2048u && s.limit < sp - 16384u + 2048u);   // the limit sits the reserve below the stack pointer, give or take this call's frame
    return test_summary();
}
