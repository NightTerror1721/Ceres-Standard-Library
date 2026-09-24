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

// The 64-bit counts are read as pairs: the low word first takes the moment and latches the high half, so the
// two reads are one instant however much the count moves in between (CeresASM e5fc11e for the ticks).
uint64_t timer_ticks64(void)
{
    unsigned int lo = mmio_r32(TIMER_TICKS_REG);
    unsigned int hi = mmio_r32(TIMER_TICKS_HIGH_REG);
    return ((uint64_t)hi << 32) | lo;
}

uint64_t timer_nanos64(void)
{
    unsigned int lo = mmio_r32(TIMER_NANOS_LOW_REG);
    unsigned int hi = mmio_r32(TIMER_NANOS_HIGH_REG);
    return ((uint64_t)hi << 32) | lo;
}

// The ns64 forms, kept for code written before the uint64_t ones.
struct ns64 timer_nanos(void)
{
    uint64_t now = timer_nanos64();
    return ns64_make((unsigned int)now, (unsigned int)(now >> 32));
}

struct ns64 timer_nanos_elapsed(struct ns64 since)
{
    uint64_t now = timer_nanos64();
    return ns64_sub(ns64_make((unsigned int)now, (unsigned int)(now >> 32)), since);
}

unsigned int timer_nanos_resolution(void)
{
    return mmio_r32(TIMER_NANOS_RES_REG);
}

unsigned int timer_halt_clock(void)
{
    return mmio_r32(TIMER_HALT_CLOCK_REG);
}

void timer_alarm_at(uint64_t at)
{
    mmio_w32(TIMER_ALARM_LOW_REG, (unsigned int)at);          // the low word first: the high one arms it
    mmio_w32(TIMER_ALARM_HIGH_REG, (unsigned int)(at >> 32));
}

uint64_t timer_alarm(void)
{
    unsigned int lo = mmio_r32(TIMER_ALARM_LOW_REG);          // 0:0 when disarmed (CeresASM 4ad3dcf)
    unsigned int hi = mmio_r32(TIMER_ALARM_HIGH_REG);
    return ((uint64_t)hi << 32) | lo;
}

// One halt with the alarm at `deadline`. A request raised before the halt runs - the alarm's among them -
// keeps it from sleeping (CeresASM 551cdbd), so an instant that comes between the look at the clock and the
// halt is not slept through. The program's own alarm is kept: one due first stays armed (and fires on time,
// ending this halt early), and one due later is put back afterwards.
int timer_halt_until_ns(uint64_t deadline)
{
    if (timer_nanos64() >= deadline)
        return 1;
    if (timer_halt_clock() == 0u)
        return 0;                                   // no real time while halted: nothing would wake it at the instant
    uint64_t saved = timer_alarm();
    int keep = saved != 0u && saved <= deadline;
    if (!keep)
        timer_alarm_at(deadline);
    __builtin_halt();
    if (!keep)
        timer_alarm_at(saved);                      // 0 disarms; a later one of the program's is armed again
    return timer_nanos64() >= deadline;
}

void timer_wait_until_ns64(uint64_t deadline)
{
    while (!timer_halt_until_ns(deadline))
    {
    }
}

void timer_wait_until_ns(struct ns64 deadline)
{
    timer_wait_until_ns64(((uint64_t)deadline.hi << 32) | deadline.lo);
}

void timer_wait_ms(unsigned int ms)
{
    timer_wait_until_ns64(timer_nanos64() + (uint64_t)ms * 1000000u);
}

void timer_wait_us(unsigned int us)
{
    timer_wait_until_ns64(timer_nanos64() + (uint64_t)us * 1000u);
}

void timer_wait_ns(unsigned int ns)
{
    timer_wait_until_ns64(timer_nanos64() + ns);
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
