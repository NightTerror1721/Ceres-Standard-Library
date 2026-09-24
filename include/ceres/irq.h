#pragma once

#include "interrupts.h"

// Interrupt handlers attached at RUN time.
//
// The hardware vector table is bound at LINK time (`__interrupt_vector`), one owner per number for
// the whole program, and a running program cannot write it (everything below 0x400 is read-only).
// So the module in src/ceres/irq.c owns the device vectors (16-23) once, and dispatches to a table
// of function pointers in RAM that irq_attach() fills in. No privilege is needed.
//
// irq_attach() is OPTIONAL to link: the module binds vectors, and a program that binds one of
// them itself (say `__interrupt_vector(17, my_isr)`) must not link it. See tools/runtests.ps1
// (`// USE: irq`) and the Makefile (`USE=irq`).
//
// A handler runs with user interrupts masked, on the 4 KiB system stack: keep it short, and do not
// call printf or malloc from it. It gets the interrupt number.

typedef void (*irq_handler_t)(int irq);

int  irq_attach(int irq, irq_handler_t handler);   // 16..24; -1 for any other number
void irq_detach(int irq);
irq_handler_t irq_handler(int irq);                // the attached handler, or NULL
const char* irq_name(int irq);                     // "Timer", "Terminal", "AlignmentFault", ...

// Critical sections. irq_save() returns whether user interrupts were enabled (16, the Interrupt
// flag's bit, or 0) and masks them; irq_restore() puts that back, so the pair nests. Both are inline:
// a read of the flags and a `cli`, and a `sti` when the state says so. The functions in asm/sys.casm
// remain for a call through a pointer.
unsigned int irq_save(void);
void         irq_restore(unsigned int state);
static inline unsigned int __irq_save(void)
{
    unsigned int state = __builtin_flags() & 16u;
    __builtin_cli();
    return state;
}
static inline void __irq_restore(unsigned int state)
{
    if (state != 0)
        __builtin_sti();
}
#define irq_save()          __irq_save()
#define irq_restore(state)  __irq_restore(state)
void         irq_enable_all(void);                 // sti

// Wait for an interrupt. irq_wait() is `sti` followed by `halt`, and the machine takes no interrupt
// between the two (`sti` takes effect after the next instruction), so an interrupt that arrives just
// before the halt wakes it rather than being handled first and leaving the halt to sleep on. A halt ends
// on any request a device raises, taken or not (CeresASM 551cdbd), so a program that only wants to sleep
// until something happens needs neither this module nor a handler: `__builtin_halt()` does it (with the
// interrupts masked first, if no handler should run). The library's waits (timer_wait_ms, getchar,
// key_wait, audio_wait) halt the same way and leave the interrupt state as they found it.
// irq_wait_flag() sleeps until *flag is set, by a handler: it reads the flag with interrupts masked,
// so it cannot miss the interrupt that sets it.
void irq_wait(void);
void irq_wait_flag(volatile int* flag);            // returns once *flag != 0
