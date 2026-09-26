// The nanosecond clock, and what is built on it. It is the machine's own time - the CPU cycles at the CPU clock -
// so the checks are about the clock moving the right way, agreeing with the millisecond clock, and waits lasting
// at least what was asked and not absurdly longer.
#include "ceres/test.h"
#include "ceres/timer.h"
#include "ceres/game.h"
#include "time.h"

int main(void)
{
    TEST_SECTION("the clock never goes backwards");
    uint64_t last = timer_nanos64();
    int backwards = 0;
    for (int i = 0; i < 2000; i++)
    {
        uint64_t now = timer_nanos64();
        if (now < last)
            backwards++;
        last = now;
    }
    CHECK_EQ(backwards, 0);

    TEST_SECTION("it agrees with the millisecond clock");
    // Both count from the machine's start on the same clock, so a millisecond reading taken before the
    // nanosecond one is never later than it, and one taken after is never earlier.
    int disagreements = 0;
    for (int i = 0; i < 200; i++)
    {
        unsigned int before = timer_millis();
        unsigned int ms = (unsigned int)(timer_nanos64() / 1000000u);
        unsigned int after = timer_millis();
        if (before > ms || ms > after)
            disagreements++;
    }
    CHECK_EQ(disagreements, 0);

    TEST_SECTION("the resolution");
    unsigned int step = timer_nanos_resolution();
    CHECK(step >= 1u);
    CHECK(step <= 1000000u);
    CHECK_EQ((int)timer_nanos_resolution(), (int)step);

    TEST_SECTION("waits last what they were asked");
    uint64_t t = timer_nanos64();
    timer_wait_us(2000);
    unsigned int us = (unsigned int)((timer_nanos64() - t) / 1000u);
    CHECK(us >= 2000u);
    CHECK(us < 500000u);
    t = timer_nanos64();
    timer_wait_ns(300000);
    unsigned int ns = (unsigned int)(timer_nanos64() - t);
    CHECK(ns >= 300000u);
    CHECK(ns < 500000000u);
    t = timer_nanos64();
    timer_wait_us(0);
    timer_wait_ns(0);
    CHECK((timer_nanos64() - t) / 1000000u < 50u);
    t = timer_nanos64();
    timer_wait_until_ns64(t + 5000000u);
    CHECK((timer_nanos64() - t) / 1000u >= 5000u);
    timer_wait_until_ns64(1u);                              // a deadline long past does not wait

    TEST_SECTION("a frame paced on the nanosecond clock");
    struct game g;
    game_init(&g, 100000000);
    game_pace_real(&g, 10);
    t = timer_nanos64();
    for (int i = 0; i < 5; i++)
    {
        game_frame_begin(&g);
        game_frame_end(&g);
    }
    unsigned int frames_us = (unsigned int)((timer_nanos64() - t) / 1000u);
    CHECK(frames_us >= 50000u);                             // five frames of 10 ms, and not a microsecond short
    CHECK(frames_us < 1000000u);
    CHECK_EQ((int)g.frame, 5);

    TEST_SECTION("timespec_get, monotonic");
    struct timespec a;
    struct timespec b;
    CHECK_EQ(timespec_get(&a, TIME_MONOTONIC), TIME_MONOTONIC);
    timer_wait_us(1500);
    CHECK_EQ(timespec_get(&b, TIME_MONOTONIC), TIME_MONOTONIC);
    CHECK(a.tv_nsec >= 0 && a.tv_nsec < 1000000000);
    CHECK(b.tv_nsec >= 0 && b.tv_nsec < 1000000000);
    long span = (long)(b.tv_sec - a.tv_sec) * 1000000000 + (b.tv_nsec - a.tv_nsec);
    CHECK(span >= 1500000);
    CHECK(span < 500000000);
    CHECK(a.tv_sec < 3600u);                                // counted from the machine's start, not from 1970

    TEST_SECTION("timespec_get, calendar time");
    struct timespec utc;
    CHECK_EQ(timespec_get(&utc, TIME_UTC), TIME_UTC);
    CHECK(utc.tv_sec > 1600000000u);
    CHECK_EQ((int)utc.tv_nsec, 0);
    CHECK((int)(utc.tv_sec - time(0)) < 2 && (int)(time(0) - utc.tv_sec) < 2);

    TEST_SECTION("timespec_get, what it refuses");
    CHECK_EQ(timespec_get(&utc, 99), 0);
    CHECK_EQ(timespec_get(0, TIME_UTC), 0);
    CHECK_EQ(timespec_getres(&utc, 99), 0);

    TEST_SECTION("timespec_getres");
    struct timespec res;
    CHECK_EQ(timespec_getres(&res, TIME_UTC), TIME_UTC);
    CHECK_EQ((int)res.tv_sec, 1);
    CHECK_EQ((int)res.tv_nsec, 0);
    CHECK_EQ(timespec_getres(&res, TIME_MONOTONIC), TIME_MONOTONIC);
    CHECK_EQ((int)res.tv_sec, 0);
    CHECK_EQ((int)res.tv_nsec, (int)step);

    return test_summary();
}
