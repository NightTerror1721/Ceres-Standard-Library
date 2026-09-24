// The real-time waits sleep: they halt with the timer's alarm armed, and a halt ends on any request, taken or
// not (CeresASM 551cdbd), so none of this needs a handler. They wake at their instant or a little after.
#include "ceres/test.h"
#include "ceres/timer.h"
#include "time.h"
#include "errno.h"

static unsigned int us_since(struct ns64 t0)
{
    return ns64_to_us(timer_nanos_elapsed(t0));
}

int main(void)
{
    TEST_SECTION("the waits take at least what they are asked for");
    CHECK(timer_halt_clock() != 0u);                    // `ceres run` keeps real time while halted
    struct ns64 t0 = timer_nanos();
    timer_wait_ms(30);
    unsigned int took = us_since(t0);
    CHECK(took >= 30000u);
    CHECK(took < 1000000u);
    t0 = timer_nanos();
    timer_wait_us(5000);
    CHECK(us_since(t0) >= 5000u);
    t0 = timer_nanos();
    timer_wait_ns(2000000);
    CHECK(us_since(t0) >= 2000u);
    CHECK(ns64_is_zero(timer_alarm()));                 // and leave the alarm as they found it

    TEST_SECTION("nanosleep");
    struct timespec req = { 0, 20000000 };
    struct timespec rem = { 5, 5 };
    t0 = timer_nanos();
    CHECK_EQ(nanosleep(&req, &rem), 0);
    CHECK(us_since(t0) >= 20000u);
    CHECK(rem.tv_sec == 0 && rem.tv_nsec == 0);
    req.tv_nsec = 1000000000;
    errno = 0;
    CHECK_EQ(nanosleep(&req, 0), -1);
    CHECK_EQ(errno, EINVAL);

    TEST_SECTION("the program's own alarm is kept");
    struct ns64 mine = ns64_add(timer_nanos(), ns64_from_sec(60));
    timer_alarm_at(mine);
    CHECK(ns64_cmp(timer_alarm(), mine) == 0);
    timer_wait_ms(5);
    CHECK(ns64_cmp(timer_alarm(), mine) == 0);          // one due later is put back
    struct ns64 soon = ns64_add(timer_nanos(), ns64_from_ms(5));
    timer_alarm_at(soon);
    timer_wait_ms(20);
    CHECK(ns64_is_zero(timer_alarm()));                 // one due first fired on its own, and is spent
    timer_alarm_at(mine);
    timer_alarm_at(ns64_from_u32(0));
    CHECK(ns64_is_zero(timer_alarm()));                 // 0 disarms

    TEST_SECTION("one halt at a time");
    CHECK(timer_halt_until_ns(timer_nanos()));          // an instant already past: no halt at all
    struct ns64 later = ns64_add(timer_nanos(), ns64_from_ms(10));
    int halts = 0;
    while (!timer_halt_until_ns(later) && halts < 1000)
        halts++;
    CHECK(ns64_cmp(timer_nanos(), later) >= 0);
    CHECK(halts < 1000);
    CHECK(ns64_is_zero(timer_alarm()));

    return test_summary();
}
