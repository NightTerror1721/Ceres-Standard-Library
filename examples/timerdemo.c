// Timers without interrupts: timer_every and timer_after register callbacks, and timer_poll() runs the ones
// that are due. The clock is the machine's instruction counter (see ceres/timer.h), so the sequence below is
// the same on every run and at every optimization level.
#include "stdio.h"
#include "ceres/timer.h"

static int ticks;
static int finished;

static void on_tick(void* ctx)
{
    ticks++;
    printf("%s %d\n", (const char*)ctx, ticks);
}

static void on_once(void* ctx)
{
    printf("%s\n", (const char*)ctx);
}

static void on_stop(void* ctx)
{
    printf("%s\n", (const char*)ctx);
    finished = 1;
}

int main(void)
{
    unsigned int start = timer_ticks();
    timer_every(3000, on_tick, "tick");          // 3000, 6000, 9000, ...
    timer_after(10000, on_once, "one-shot at 10000");
    int stop = timer_after(16000, on_stop, "stop at 16000");
    printf("%d timers waiting\n", timer_pending());

    while (!finished)
        timer_poll();

    timer_cancel(stop);                            // safe on one that already ran
    printf("%d timer left (the repeating one)\n", timer_pending());
    printf("took at least %s\n", timer_elapsed(start) >= 16000u ? "16000 ticks" : "less?!");
    return 0;
}
