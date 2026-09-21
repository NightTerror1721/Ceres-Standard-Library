#include "ceres/timer.h"

unsigned int timer_ticks(void)
{
    return mmio_r32(TIMER_TICKS_REG);
}

unsigned int timer_clock(void)
{
    return mmio_r32(TIMER_CLOCK_REG);
}

unsigned int timer_elapsed(unsigned int since)
{
    return mmio_r32(TIMER_TICKS_REG) - since;      // unsigned subtraction is right across the wrap
}

unsigned int timer_millis(void)
{
    return mmio_r32(TIMER_MILLIS_REG);
}

unsigned int timer_millis_elapsed(unsigned int since)
{
    return mmio_r32(TIMER_MILLIS_REG) - since;
}

void timer_wait_ms(unsigned int ms)
{
    unsigned int start = mmio_r32(TIMER_MILLIS_REG);
    while (mmio_r32(TIMER_MILLIS_REG) - start < ms)
    {
    }
}

void timer_arm(unsigned int ticks, int periodic)
{
    if (ticks == 0)
        ticks = 1;                                  // 0 would disarm it
    if (ticks > TIMER_MAX_TICKS)
        ticks = TIMER_MAX_TICKS;
    mmio_w32(TIMER_CMD_REG, ticks | (periodic ? TIMER_PERIODIC : 0u));
}

void timer_disarm(void)
{
    mmio_w32(TIMER_CMD_REG, 0u);
}

void timer_wait(unsigned int ticks)
{
    unsigned int start = mmio_r32(TIMER_TICKS_REG);
    while (mmio_r32(TIMER_TICKS_REG) - start < ticks)
    {
    }
}

void timer_wait_until(unsigned int deadline)
{
    // "reached" is a signed difference, so a deadline just past the wrap still counts as the future
    while ((int)(mmio_r32(TIMER_TICKS_REG) - deadline) < 0)
    {
    }
}

// ---- software timers ----

struct timer_task
{
    timer_cb cb;                 // NULL = free slot
    void* ctx;
    unsigned int deadline;       // when it is next due
    unsigned int period;         // 0 = one-shot
};

static struct timer_task tasks[TIMER_MAX_TASKS];

static int add_task(unsigned int ticks, timer_cb cb, void* ctx, unsigned int period)
{
    if (cb == 0)
        return -1;
    for (int i = 0; i < TIMER_MAX_TASKS; i++)
    {
        if (tasks[i].cb == 0)
        {
            tasks[i].cb = cb;
            tasks[i].ctx = ctx;
            tasks[i].deadline = mmio_r32(TIMER_TICKS_REG) + ticks;
            tasks[i].period = period;
            return i;
        }
    }
    return -1;
}

int timer_after(unsigned int ticks, timer_cb cb, void* ctx)
{
    return add_task(ticks, cb, ctx, 0);
}

int timer_every(unsigned int ticks, timer_cb cb, void* ctx)
{
    if (ticks == 0)
        ticks = 1;                                  // a zero period would run on every poll forever
    return add_task(ticks, cb, ctx, ticks);
}

void timer_cancel(int id)
{
    if (id >= 0 && id < TIMER_MAX_TASKS)
        tasks[id].cb = 0;
}

int timer_pending(void)
{
    int n = 0;
    for (int i = 0; i < TIMER_MAX_TASKS; i++)
        if (tasks[i].cb != 0)
            n++;
    return n;
}

int timer_poll(void)
{
    unsigned int now = mmio_r32(TIMER_TICKS_REG);
    int ran = 0;
    // Each poll runs the tasks that are due, the one that has waited longest first. A callback may
    // schedule or cancel tasks (including itself), so the table is looked at afresh every round.
    for (;;)
    {
        int pick = -1;
        unsigned int oldest = 0;                    // how far past its deadline the pick is
        for (int i = 0; i < TIMER_MAX_TASKS; i++)
        {
            if (tasks[i].cb == 0)
                continue;
            unsigned int late = now - tasks[i].deadline;
            if ((int)late < 0)
                continue;                           // not due yet
            if (pick < 0 || late > oldest)
            {
                pick = i;
                oldest = late;
            }
        }
        if (pick < 0)
            return ran;

        timer_cb cb = tasks[pick].cb;
        void* ctx = tasks[pick].ctx;
        if (tasks[pick].period != 0)
            tasks[pick].deadline += tasks[pick].period;   // keeps its rhythm even when polled late
        else
            tasks[pick].cb = 0;                     // one-shot: free the slot BEFORE running it
        cb(ctx);
        ran++;
        if (ran >= 4 * TIMER_MAX_TASKS)
            return ran;                             // a period shorter than the polling gap must not trap us here
    }
}
