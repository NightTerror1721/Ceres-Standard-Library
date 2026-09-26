# `<ceres/timer.h>`

Timer device (0xFF010000). See CeresASM docs/07-IO-Devices-and-Ports.md.

TIME HERE IS THE MACHINE'S OWN. The CPU counts cycles - each instruction costs a fixed number of them (1 for an addition, 2 for a load or a store, 16 for a division...) - and every clock the timer has is worked out from that count at the CPU clock, timer_cpu_hz() (50 MHz by default): the nanoseconds, the milliseconds and the real-time clock alike. So a program reads the same instants on every run and on every host, and how that time keeps pace with the wall clock is the host's business (`ceres run --speed`), not the program's.

A TICK IS A CPU CYCLE: timer_ticks(), timer_elapsed(), timer_arm() and the tick waits count cycles, so a frame budget or a span measures work in the machine's own units - the same program costs more cycles at -O0 than at -O2, and the same number on every run. timer_cycles64() is the whole 64-bit count.

The real-time clock, timer_clock(), is seconds since 1970: where the machine's time started (the host's clock when the machine started, or `ceres run --rtc`) plus the machine's time since.

A program that HALTs does not stop the clock: the machine jumps straight to the next thing a device has scheduled - the timer running out, the alarm, a transfer landing - and its time moves on by as much. With nothing scheduled only the host can wake it (a key, input).

Nothing here needs an interrupt handler, so this module never binds a vector. The tick waits (timer_wait, timer_wait_until) spin on the cycle count. The time waits (timer_wait_ms, _us, _ns, _until_ns64, and sleep() and nanosleep() above them) SLEEP: they arm the timer's ALARM - an absolute instant on the nanosecond clock, which raises interrupt 24 - and halt. A halt ends on any request a device raises, taken or not, with interrupts masked or enabled (CeresASM 551cdbd), so no handler is needed; the wait looks at the clock each time and halts again until its instant. The task table (timer_after/every) is driven by timer_poll().

```c
// The registers (CeresASM plan/v2 SPEC 5.7).
#define TIMER_CYCLES_LOW_REG   (TIMER_BASE + 0x00)   // R: CPU cycles since the start, low word; latches the high word
#define TIMER_CYCLES_HIGH_REG  (TIMER_BASE + 0x04)   // R: the high word latched by the last low read
#define TIMER_COUNTDOWN_REG    (TIMER_BASE + 0x08)   // RW: N cycles until IRQ_TIMER; 0 disarms; reads what is left
#define TIMER_CONTROL_REG      (TIMER_BASE + 0x0C)   // RW: TIMER_PERIODIC re-arms with the last countdown written
#define TIMER_NANOS_LOW_REG    (TIMER_BASE + 0x10)   // R: nanoseconds since the start, low word; latches the high word
#define TIMER_NANOS_HIGH_REG   (TIMER_BASE + 0x14)   // R: the high word latched by the last read of the low one
#define TIMER_MILLIS_REG       (TIMER_BASE + 0x18)   // R: milliseconds since the start (wraps at 49 days)
#define TIMER_RTC_REG          (TIMER_BASE + 0x1C)   // R: seconds since 1970, the start plus the machine's time
#define TIMER_ALARM_LOW_REG    (TIMER_BASE + 0x20)   // RW: the alarm instant in nanoseconds, low word
#define TIMER_ALARM_HIGH_REG   (TIMER_BASE + 0x24)   // RW: the high word; writing it arms the alarm (0:0 disarms)
#define TIMER_CPU_HZ_REG       (TIMER_BASE + 0x28)   // R: the CPU clock, cycles per second
#define TIMER_PERIODIC   0x1u                      // TIMER_CONTROL_REG bit 0
#define TIMER_MAX_TICKS  0xFFFFFFFFu               // the longest period the countdown register can hold

unsigned int timer_ticks(void);                          // CPU cycles so far (wraps at 2^32: 86 s at 50 MHz)
uint64_t     timer_cycles64(void);                       // the same, all 64 bits: what a run can compare with another
uint64_t     timer_nanos64(void);                        // nanoseconds since the machine started (584 years before it wraps)
unsigned int timer_clock(void);                          // the real-time clock: seconds since 1970
unsigned int timer_elapsed(unsigned int since);          // cycles since `since`, correct across the wrap
unsigned int timer_millis(void);                         // milliseconds since the machine started
unsigned int timer_millis_elapsed(unsigned int since);   // milliseconds since `since`, correct across the wrap
unsigned int timer_nanos_resolution(void);               // the clock's step: one CPU cycle, in nanoseconds (never 0)
unsigned int timer_cpu_hz(void);                         // the CPU clock, cycles per second

// The hardware timer raises interrupt 16 (IRQ_TIMER) when it expires; it is masked unless the
// program has done sti and attached a handler (ceres/irq.h).
void timer_arm(unsigned int ticks, int periodic);        // fire in `ticks` cycles (1..TIMER_MAX_TICKS)
void timer_disarm(void);

// Waiting on ticks: a spin on the cycle count, so it needs no interrupt and is exact to a few instructions.
void timer_wait(unsigned int ticks);                     // return after `ticks` cycles have passed
void timer_wait_until(unsigned int deadline);            // return once timer_ticks() has reached `deadline`

// Waiting in time: the machine sleeps (see above) and wakes on the cycle the instant falls on.
// timer_halt_until_ns is one halt of such a wait, for a loop that also waits for something else: it returns
// early when any device raises a request (a key, a transfer done).
void timer_wait_ms(unsigned int ms);                     // return after `ms` milliseconds
void timer_wait_us(unsigned int us);                     // return after `us` microseconds
void timer_wait_ns(unsigned int ns);                     // return after `ns` nanoseconds (at most 4.29 s)
void timer_wait_until_ns64(uint64_t deadline);           // return once timer_nanos64() has reached `deadline`
int  timer_halt_until_ns(uint64_t deadline);             // one halt, until `deadline` or any request; nonzero once it has passed

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
```
