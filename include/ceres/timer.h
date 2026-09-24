#pragma once

#include "../ceres.h"
#include "../stdint.h"
#include "../interrupts.h"
#include "ns64.h"

// Timer device (0xFF010000). See CeresASM docs/07-IO-Devices-and-Ports.md.
//
// TIME HERE IS COUNTED IN INSTRUCTIONS. A tick is one instruction executed - or, while the CPU is halted,
// one tick of the halted clock (below), an instruction's worth of time. A program that never halts ticks
// the same number of times on every run, which is what makes tests reproducible; one that halts until an
// outside event (a key, a real-time frame) ticks as long as it waited. An optimized build does more work
// per tick and a game loop must measure WORK, not time. The wall clock (timer_clock, seconds since 1970) is the one value in the whole
// machine that is not deterministic - and so is the millisecond register: real time, for the code that wants
// to keep a rhythm on the wall clock.
//
// The nanosecond clock is the exact one: 64 bits, from the host's steady clock, read with timer_nanos64() as a
// uint64_t. (timer_nanos() and the rest of the struct ns64 forms are kept for code written before, and are
// deprecated: a uint64_t does the same arithmetic with the ordinary operators.)
// It is real time too, so it is as non-deterministic as the millisecond one, and a debugger replays it. What it
// can tell apart is the host clock's step, timer_nanos_resolution(): often 100 ns, so two reads a few
// instructions apart may return the same count.
//
// A program that HALTs with the timer armed does not stop the count: while the CPU is halted the clock runs
// on at timer_halt_clock() ticks per second (100 000 000 by default, about what the machine executes), so a
// tick is an instruction's worth of time either way. That is how a real-time wait becomes ticks for a halt:
// 16 ms is timer_halt_clock() / 1000 * 16 of them. A host that does not run the halted clock in real time (a
// debugger replaying) reports 0.
//
// Nothing here needs an interrupt handler, so this module never binds a vector. The tick waits (timer_wait,
// timer_wait_until) spin on the tick register, exact to a few instructions. The real-time waits (timer_wait_ms,
// _us, _ns, _until_ns, and sleep() and nanosleep() above them) SLEEP: they arm the timer's ALARM - an absolute
// instant on the nanosecond clock, which raises interrupt 24 - and halt. A halt ends on any request a device
// raises, taken or not, with interrupts masked or enabled (CeresASM 551cdbd), so the host is not kept busy and
// no handler is needed; the wait looks at the clock each time and halts again until its instant. A host that
// does not keep real time while halted (timer_halt_clock() == 0, a debugger replaying) cannot wake a halt at an
// instant, and there they spin. The task table (timer_after/every) is driven by timer_poll().

#define TIMER_TICKS_REG  (TIMER_BASE + 0x00)   // R: the low word of the ticks: instructions executed, and halted-clock ticks; latches the high word
#define TIMER_CLOCK_REG  (TIMER_BASE + 0x04)   // R: wall-clock seconds since 1970
#define TIMER_CMD_REG    (TIMER_BASE + 0x08)   // W: N instructions until it fires; bit 31 = periodic; 0 disarms
#define TIMER_MILLIS_REG (TIMER_BASE + 0x0C)   // R: milliseconds since the machine started (wraps at 49 days)
#define TIMER_NANOS_LOW_REG  (TIMER_BASE + 0x10)   // R: the low word of the nanoseconds since the machine started; latches the high word
#define TIMER_NANOS_HIGH_REG (TIMER_BASE + 0x14)   // R: the high word latched by the last read of the low one
#define TIMER_NANOS_RES_REG  (TIMER_BASE + 0x18)   // R: the smallest step the nanosecond clock is seen to take, in nanoseconds
#define TIMER_HALT_CLOCK_REG (TIMER_BASE + 0x1C)   // R: ticks per second while the CPU is halted; 0 when not in real time
#define TIMER_ALARM_LOW_REG  (TIMER_BASE + 0x20)   // RW: the low word of the alarm instant (nanoseconds, NANOS' clock)
#define TIMER_ALARM_HIGH_REG (TIMER_BASE + 0x24)   // RW: the high word; writing it arms the alarm (0:0 disarms)
#define TIMER_TICKS_HIGH_REG (TIMER_BASE + 0x28)   // R: the high word of the ticks latched by the last low read
#define TIMER_PERIODIC   0x80000000u
#define TIMER_MAX_TICKS  0x7FFFFFFFu           // the longest period the command register can hold

unsigned int timer_ticks(void);                          // ticks so far: instructions, and halted time (wraps at 2^32, about 43 s)
uint64_t     timer_ticks64(void);                        // the same, all 64 bits: the count of instructions a run can compare
uint64_t     timer_nanos64(void);                        // nanoseconds since the machine started (584 years before it wraps)
unsigned int timer_clock(void);                          // wall-clock seconds since 1970
unsigned int timer_elapsed(unsigned int since);          // ticks since `since`, correct across the wrap
unsigned int timer_millis(void);                         // wall-clock milliseconds since the machine started
unsigned int timer_millis_elapsed(unsigned int since);   // milliseconds since `since`, correct across the wrap
struct ns64  timer_nanos(void) __attribute__((__deprecated__));                       // timer_nanos64(), as an ns64
struct ns64  timer_nanos_elapsed(struct ns64 since) __attribute__((__deprecated__));   // timer_nanos64() - since
unsigned int timer_nanos_resolution(void);               // the clock's step in nanoseconds (never 0)
unsigned int timer_halt_clock(void);                     // ticks per second while halted (0: the host does not keep real time)

// The hardware timer raises interrupt 16 (IRQ_TIMER) when it expires; it is masked unless the
// program has done sti and attached a handler (ceres/irq.h).
void timer_arm(unsigned int ticks, int periodic);        // fire in `ticks` instructions (1..TIMER_MAX_TICKS)
void timer_disarm(void);

// Waiting on ticks: a spin on the tick register, so it needs no interrupt and is exact to a few instructions.
void timer_wait(unsigned int ticks);                     // return after `ticks` instructions have passed
void timer_wait_until(unsigned int deadline);            // return once timer_ticks() has reached `deadline`

// Waiting in real time: the machine sleeps (see above), and wakes at the instant or a little after - the host
// sleeps in its own steps, often a millisecond. timer_halt_until_ns is one halt of such a wait, for a loop that
// also waits for something else: it returns early when any device raises a request (a key, a transfer done).
void timer_wait_ms(unsigned int ms);                     // return after `ms` real milliseconds
void timer_wait_us(unsigned int us);                     // return after `us` real microseconds
void timer_wait_ns(unsigned int ns);                     // return after `ns` real nanoseconds (at most 4.29 s)
void timer_wait_until_ns64(uint64_t deadline);           // return once timer_nanos64() has reached `deadline`
int  timer_halt_until_ns(uint64_t deadline);             // one halt, until `deadline` or any request; nonzero once it has passed
void timer_wait_until_ns(struct ns64 deadline) __attribute__((__deprecated__));       // timer_wait_until_ns64, from an ns64

// The alarm itself. The waits above use it and put back whatever it was set to, so a program may keep one of
// its own: one due before a wait's instant still fires on time.
void     timer_alarm_at(uint64_t at);                    // raise IRQ_ALARM once timer_nanos64() reaches `at` (0 disarms)
uint64_t timer_alarm(void);                              // the armed instant, 0 when disarmed

// A table of software timers driven by polling: call timer_poll() from the main loop and every
// task whose time has come runs, in the order they fell due. Up to TIMER_MAX_TASKS at once.
#include "config.h"                            // TIMER_MAX_TASKS, unless the build sets it

typedef void (*timer_cb)(void* ctx);

int  timer_after(unsigned int ticks, timer_cb cb, void* ctx);   // once; an id >= 0, or -1 when the table is full
int  timer_every(unsigned int ticks, timer_cb cb, void* ctx);   // repeatedly, starting `ticks` from now
void timer_cancel(int id);                                      // safe on a finished or invalid id
int  timer_pending(void);                                       // tasks still scheduled
int  timer_poll(void);                                          // runs what is due; returns how many ran
