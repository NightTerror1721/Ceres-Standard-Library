// Wall-clock frame pacing by halting. Part of the irq module: it attaches a handler to the timer
// interrupt, which is bound once per program. See ceres/game.h.
#include "ceres/game.h"
#include "ceres/irq.h"
#include "ceres/timer.h"

static volatile int period_over;

static void on_timer(int irq)
{
    period_over = 1;
}

void __game_sleep(unsigned int ticks);   // asm/optional/game_wait.casm

// A halted machine advances the timer one tick per millisecond, so waiting `wait_ms` ticks is about that many
// milliseconds and costs the host next to nothing. Any other interrupt (a key, the mouse) wakes the halt
// early: then what is left of the period is slept, from how far the tick counter has moved. Within 8 ticks of
// the end the wait is over: __game_sleep needs at least 8 for its timer not to expire before it halts.
static void wait_halted(struct game* g)
{
    unsigned int start = timer_ticks();
    period_over = 0;
    for (;;)
    {
        unsigned int spent = timer_elapsed(start);
        if (period_over || spent + 8u >= g->wait_ms)
            break;
        __game_sleep(g->wait_ms - spent);
    }
    timer_disarm();
}

void game_pace_ms(struct game* g, unsigned int ms)
{
    if (ms < 10)
        ms = 10;                            // shorter periods cannot be slept reliably
    irq_attach(IRQ_TIMER, on_timer);
    g->wait_ms = ms;
    g->wait = wait_halted;
}
