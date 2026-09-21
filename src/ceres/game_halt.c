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

// A halted machine advances the timer one tick per step, and sleeps a millisecond or so per step, so a short
// sleep costs the host next to nothing. What decides when the frame is over is the nanosecond clock, not
// the ticks: the sleep is a few ticks long and the loop looks at the clock each time it wakes, so the frame
// ends on time whatever a step really takes, and any other interrupt (a key, the mouse) that wakes the halt
// early just brings the loop round again. Four ticks: the store, the sti and the halt take three of them,
// which leaves one halted step.
#define SLEEP_TICKS 4u

static void wait_halted(struct game* g)
{
    period_over = 0;
    struct ns64 deadline = ns64_add(g->frame_start_ns, ns64_from_ms(g->wait_ms));
    while (ns64_cmp(timer_nanos(), deadline) < 0)
        __game_sleep(SLEEP_TICKS);
    timer_disarm();
}

void game_pace_ms(struct game* g, unsigned int ms)
{
    if (ms < 1)
        ms = 1;
    irq_attach(IRQ_TIMER, on_timer);
    g->wait_ms = ms;
    g->wait = wait_halted;
}
