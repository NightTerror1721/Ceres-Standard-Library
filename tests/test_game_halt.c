// USE: irq
// Frame pacing by halting: game_pace_ms waits with the timer armed, so the machine sleeps (a halted
// machine's clock runs on at timer_halt_clock() ticks per second) instead of spinning on a budget.
#include "ceres/test.h"
#include "ceres/game.h"
#include "ceres/timer.h"
#include "ceres/irq.h"

int main(void)
{
    struct game g;
    game_init(&g, 100000000);                           // a budget that a spin could never finish

    TEST_SECTION("set up");
    game_pace_ms(&g, 20);
    CHECK_EQ((int)g.wait_ms, 20);
    CHECK(g.wait != 0);
    CHECK(irq_handler(IRQ_TIMER) != 0);
    game_pace_ms(&g, 0);                                // the shortest period is a millisecond
    CHECK_EQ((int)g.wait_ms, 1);
    game_pace_ms(&g, 20);

    TEST_SECTION("frames pass on the clock, not on instructions");
    CHECK(timer_halt_clock() >= 1000u);                 // `ceres run` keeps real time while halted (100 MHz by default)
    unsigned int per_ms = timer_halt_clock() / 1000u;
    unsigned int s0 = timer_clock();
    int long_enough = 1, short_enough = 1;
    for (int i = 0; i < 10; i++)
    {
        game_frame_begin(&g);
        unsigned int t0 = timer_ticks();
        game_frame_end(&g);
        unsigned int waited = timer_elapsed(t0);
        if (waited < 12u * per_ms) long_enough = 0;     // about 20 ms of the halted clock
        if (waited > 60u * per_ms) short_enough = 0;    // and not much more: the frame is 20 ms
    }
    CHECK_EQ((int)g.frame, 10);
    CHECK(long_enough);
    CHECK(short_enough);
    CHECK(timer_clock() - s0 <= 2u);                    // about 200 ms of wall time

    TEST_SECTION("the timer is left disarmed");
    unsigned int before = timer_ticks();
    for (int i = 0; i < 50; i++) { }
    CHECK(timer_elapsed(before) < 5000u);               // no stray timer wait is pending

    return test_summary();
}
