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

// While the CPU is halted the machine's clock runs on at timer_halt_clock() ticks per second, and the host
// sleeps until the timer expires. So the rest of the frame, in nanoseconds, becomes one sleep of that many
// ticks. What decides when the frame is over is still the nanosecond clock: the loop looks at it each time
// the halt ends, so a key or the mouse waking it early just brings the loop round for the time that is left.
// A host that does not keep real time while halted (a debugger replaying) reports a rate of 0: then the
// sleep is a few ticks and the loop watches the clock, as it did before the halted clock existed. The store,
// the sti and the halt take three ticks of any sleep, so none is shorter than four.
#define MIN_SLEEP_TICKS 4u

static unsigned int ticks_for(struct ns64 left, unsigned int rate)
{
    if (rate == 0u)
        return MIN_SLEEP_TICKS;
    unsigned long long ns = ((unsigned long long)left.hi << 32) | (unsigned long long)left.lo;
    // ns * rate / 1e9 without overflowing: a frame's rest is well under a second, but a stalled one need not be
    unsigned long long ticks = (ns / 1000000000ull) * rate + (ns % 1000000000ull) * rate / 1000000000ull;
    if (ticks < MIN_SLEEP_TICKS)
        return MIN_SLEEP_TICKS;
    if (ticks > TIMER_MAX_TICKS)
        return TIMER_MAX_TICKS;
    return (unsigned int)ticks;
}

static void wait_halted(struct game* g)
{
    period_over = 0;
    unsigned int rate = timer_halt_clock();
    struct ns64 deadline = ns64_add(g->frame_start_ns, ns64_from_ms(g->wait_ms));
    for (;;)
    {
        struct ns64 now = timer_nanos();
        if (ns64_cmp(now, deadline) >= 0)
            break;
        __game_sleep(ticks_for(ns64_sub(deadline, now), rate));
    }
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
