// The real-time waits sleep: they halt with the timer's alarm armed, and a halt ends on any request, taken or
// not (CeresASM 551cdbd), so none of this needs a handler. They wake at their instant or a little after.
#include "ceres/test.h"
#include "ceres/timer.h"
#include "time.h"
#include "errno.h"

static uint64_t us_since(uint64_t t0)
{
    return (timer_nanos64() - t0) / 1000u;
}

int main(void)
{
    TEST_SECTION("the waits take at least what they are asked for");
    CHECK(timer_halt_clock() != 0u);                    // `ceres run` keeps real time while halted
    uint64_t t0 = timer_nanos64();
    timer_wait_ms(30);
    uint64_t took = us_since(t0);
    CHECK(took >= 30000u);
    CHECK(took < 1000000u);
    t0 = timer_nanos64();
    timer_wait_us(5000);
    CHECK(us_since(t0) >= 5000u);
    t0 = timer_nanos64();
    timer_wait_ns(2000000);
    CHECK(us_since(t0) >= 2000u);
    CHECK(timer_alarm() == 0u);                         // and leave the alarm as they found it

    TEST_SECTION("nanosleep");
    struct timespec req = { 0, 20000000 };
    struct timespec rem = { 5, 5 };
    t0 = timer_nanos64();
    CHECK_EQ(nanosleep(&req, &rem), 0);
    CHECK(us_since(t0) >= 20000u);
    CHECK(rem.tv_sec == 0 && rem.tv_nsec == 0);
    req.tv_nsec = 1000000000;
    errno = 0;
    CHECK_EQ(nanosleep(&req, 0), -1);
    CHECK_EQ(errno, EINVAL);
    req.tv_sec = -1;                                    // time_t is signed now (M12): a negative time is an error
    req.tv_nsec = 0;
    errno = 0;
    CHECK_EQ(nanosleep(&req, 0), -1);
    CHECK_EQ(errno, EINVAL);

    TEST_SECTION("the program's own alarm is kept");
    uint64_t mine = timer_nanos64() + 60000000000ull;
    timer_alarm_at(mine);
    CHECK(timer_alarm() == mine);
    timer_wait_ms(5);
    CHECK(timer_alarm() == mine);                       // one due later is put back
    timer_alarm_at(timer_nanos64() + 5000000u);
    timer_wait_ms(20);
    CHECK(timer_alarm() == 0u);                         // one due first fired on its own, and is spent
    timer_alarm_at(mine);
    timer_alarm_at(0);
    CHECK(timer_alarm() == 0u);                         // 0 disarms

    TEST_SECTION("one halt at a time");
    CHECK(timer_halt_until_ns(timer_nanos64()));        // an instant already past: no halt at all
    uint64_t later = timer_nanos64() + 10000000u;
    int halts = 0;
    while (!timer_halt_until_ns(later) && halts < 1000)
        halts++;
    CHECK(timer_nanos64() >= later);
    CHECK(halts < 1000);
    CHECK(timer_alarm() == 0u);

    return test_summary();
}
