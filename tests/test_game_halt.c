// USE: irq
// Frame pacing by halting: game_pace_ms waits with the timer armed, so the machine sleeps (a halted
// machine keeps time at one tick per millisecond) instead of spinning on a budget.
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
    game_pace_ms(&g, 0);                                // shorter than 10 ms cannot be slept reliably
    CHECK_EQ((int)g.wait_ms, 10);
    game_pace_ms(&g, 20);

    TEST_SECTION("frames pass on the clock, not on instructions");
    unsigned int s0 = timer_clock();
    int long_enough = 1, short_enough = 1;
    for (int i = 0; i < 10; i++)
    {
        game_frame_begin(&g);
        unsigned int t0 = timer_ticks();
        game_frame_end(&g);
        unsigned int waited = timer_elapsed(t0);
        if (waited < 12u) long_enough = 0;              // about 20 ticks of sleep
        if (waited > 100000u) short_enough = 0;         // the interrupt handler costs thousands at -O0, but nowhere near the 100000000 a spin would need
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
