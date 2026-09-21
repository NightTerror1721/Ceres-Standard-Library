#pragma once

#include "../ceres.h"
#include "ns64.h"

// Timer device (0xFF010000). See CeresASM docs/07-IO-Devices-and-Ports.md.
//
// TIME HERE IS COUNTED IN INSTRUCTIONS. The tick register counts instructions executed, not
// milliseconds: the same program ticks the same number of times on every run, which is what makes
// tests reproducible, but an optimized build does more work per tick and a game loop must measure
// WORK, not time. The wall clock (timer_clock, seconds since 1970) is the one value in the whole
// machine that is not deterministic - and so is the millisecond register: real time, for the code that wants
// to keep a rhythm on the wall clock.
//
// The nanosecond clock is the exact one: 64 bits, read as a pair (ceres/ns64.h), from the host's steady clock.
// It is real time too, so it is as non-deterministic as the millisecond one, and a debugger replays it. What it
// can tell apart is the host clock's step, timer_nanos_resolution(): often 100 ns, so two reads a few
// instructions apart may return the same count.
//
// Nothing here needs an interrupt handler, so this module never binds a vector: the waits spin on
// the tick register and the task table (timer_after/every) is driven by timer_poll().

#define TIMER_TICKS_REG  (TIMER_BASE + 0x00)   // R: instructions executed (truncated to 32 bits)
#define TIMER_CLOCK_REG  (TIMER_BASE + 0x04)   // R: wall-clock seconds since 1970
#define TIMER_CMD_REG    (TIMER_BASE + 0x08)   // W: N instructions until it fires; bit 31 = periodic; 0 disarms
#define TIMER_MILLIS_REG (TIMER_BASE + 0x0C)   // R: milliseconds since the machine started (wraps at 49 days)
#define TIMER_NANOS_LOW_REG  (TIMER_BASE + 0x10)   // R: the low word of the nanoseconds since the machine started; latches the high word
#define TIMER_NANOS_HIGH_REG (TIMER_BASE + 0x14)   // R: the high word latched by the last read of the low one
#define TIMER_NANOS_RES_REG  (TIMER_BASE + 0x18)   // R: the smallest step the nanosecond clock is seen to take, in nanoseconds
#define TIMER_PERIODIC   0x80000000u
#define TIMER_MAX_TICKS  0x7FFFFFFFu           // the longest period the command register can hold

unsigned int timer_ticks(void);                          // instructions executed so far (wraps at 2^32)
unsigned int timer_clock(void);                          // wall-clock seconds since 1970
unsigned int timer_elapsed(unsigned int since);          // ticks since `since`, correct across the wrap
unsigned int timer_millis(void);                         // wall-clock milliseconds since the machine started
unsigned int timer_millis_elapsed(unsigned int since);   // milliseconds since `since`, correct across the wrap
struct ns64  timer_nanos(void);                          // nanoseconds since the machine started: 584 years before it wraps
struct ns64  timer_nanos_elapsed(struct ns64 since);     // nanoseconds since `since`
unsigned int timer_nanos_resolution(void);               // the clock's step in nanoseconds (never 0)

// The hardware timer raises interrupt 16 (IRQ_TIMER) when it expires; it is masked unless the
// program has done sti and attached a handler (ceres/irq.h).
void timer_arm(unsigned int ticks, int periodic);        // fire in `ticks` instructions (1..TIMER_MAX_TICKS)
void timer_disarm(void);

// Waiting: a spin on the tick register, so it needs no interrupt and is exact to a few instructions.
void timer_wait(unsigned int ticks);                     // return after `ticks` instructions have passed
void timer_wait_until(unsigned int deadline);            // return once timer_ticks() has reached `deadline`
void timer_wait_ms(unsigned int ms);                     // return after `ms` real milliseconds: a spin on the clock
void timer_wait_us(unsigned int us);                     // return after `us` real microseconds
void timer_wait_ns(unsigned int ns);                     // return after `ns` real nanoseconds (at most 4.29 s; the clock's step limits how exactly)
void timer_wait_until_ns(struct ns64 deadline);          // return once timer_nanos() has reached `deadline`

// A table of software timers driven by polling: call timer_poll() from the main loop and every
// task whose time has come runs, in the order they fell due. Up to TIMER_MAX_TASKS at once.
#include "config.h"                            // TIMER_MAX_TASKS, unless the build sets it

typedef void (*timer_cb)(void* ctx);

int  timer_after(unsigned int ticks, timer_cb cb, void* ctx);   // once; an id >= 0, or -1 when the table is full
int  timer_every(unsigned int ticks, timer_cb cb, void* ctx);   // repeatedly, starting `ticks` from now
void timer_cancel(int id);                                      // safe on a finished or invalid id
int  timer_pending(void);                                       // tasks still scheduled
int  timer_poll(void);                                          // runs what is due; returns how many ran
