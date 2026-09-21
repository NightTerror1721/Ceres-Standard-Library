// The millisecond register, and what is built on it. It is real time, so nothing here checks an exact
// duration: only that the clock moves the right way and that waits last at least what was asked, and not
// absurdly longer.
#include "ceres/test.h"
#include "ceres/timer.h"
#include "ceres/game.h"
#include "time.h"

int main(void)
{
    TEST_SECTION("the clock");
    unsigned int a = timer_millis();
    unsigned int b = timer_millis();
    CHECK(b - a < 50u);                                 // never backwards (an unsigned difference), and quick
    CHECK(timer_millis_elapsed(a) < 5000u);

    TEST_SECTION("timer_wait_ms");
    unsigned int t = timer_millis();
    timer_wait_ms(40);
    unsigned int d = timer_millis_elapsed(t);
    CHECK(d >= 40u);
    CHECK(d < 1000u);
    t = timer_millis();
    timer_wait_ms(0);
    CHECK(timer_millis_elapsed(t) < 50u);

    TEST_SECTION("sleep(0) does not wait");
    t = timer_millis();
    sleep(0);
    CHECK(timer_millis_elapsed(t) < 50u);

    TEST_SECTION("game_pace_real");
    struct game g;
    game_init(&g, 100000000);
    game_pace_real(&g, 15);
    CHECK_EQ((int)g.wait_ms, 15);
    t = timer_millis();
    for (int i = 0; i < 4; i++)
    {
        game_frame_begin(&g);
        game_frame_end(&g);
    }
    d = timer_millis_elapsed(t);
    CHECK(d >= 60u);                                    // four frames of 15 ms
    CHECK(d < 1000u);
    CHECK_EQ((int)g.frame, 4);

    TEST_SECTION("a late frame is not repaid");
    game_frame_begin(&g);
    timer_wait_ms(40);                                  // the frame's own work outlasts its 15 ms
    t = timer_millis();
    game_frame_end(&g);
    CHECK(timer_millis_elapsed(t) < 10u);               // so it does not wait at all

    return test_summary();
}
