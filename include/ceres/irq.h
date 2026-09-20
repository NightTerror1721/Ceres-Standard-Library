#pragma once

#include "interrupts.h"

// Interrupt handlers attached at RUN time.
//
// The hardware vector table is bound at LINK time (`__interrupt_vector`), one owner per number for
// the whole program, and a running program cannot write it (everything below 0x400 is read-only).
// So the module in src/ceres/irq.c owns the device vectors (16-21) once, and dispatches to a table
// of function pointers in RAM that irq_attach() fills in. No privilege is needed.
//
// irq_attach() is OPTIONAL to link: the module binds vectors, and a program that binds one of
// them itself (say `__interrupt_vector(17, my_isr)`) must not link it. See tools/runtests.ps1
// (`// USE: irq`) and the Makefile (`USE=irq`).
//
// A handler runs with user interrupts masked, on the 1 KiB system stack: keep it short, and do not
// call printf or malloc from it. It gets the interrupt number.

#define IRQ_COUNT       64
#define IRQ_USER_FIRST  16
#define IRQ_DEVICE_LAST 21     // the last number a device raises today (gamepad)

typedef void (*irq_handler_t)(int irq);

int  irq_attach(int irq, irq_handler_t handler);   // 16..21; -1 for any other number
void irq_detach(int irq);
irq_handler_t irq_handler(int irq);                // the attached handler, or NULL
const char* irq_name(int irq);                     // "Timer", "Terminal", "AlignmentFault", ...

// Critical sections, in asm/sys.casm. irq_save() returns whether user interrupts were enabled and
// masks them; irq_restore() puts that back, so the pair nests.
unsigned int irq_save(void);
void         irq_restore(unsigned int state);
void         irq_enable_all(void);                 // sti

// Wait for an interrupt. `sti` followed by `halt` is NOT atomic: an interrupt that arrives between
// the two is handled and then the halt sleeps until the NEXT one (a lost wake-up), because the
// machine has no shadow after `sti`. irq_wait() is that bare pair. irq_wait_flag() re-checks a
// flag around it and, when nothing is attached to the timer, arms a short one-shot timer as a
// guard so the sleep is always bounded.
void irq_wait(void);
void irq_wait_flag(volatile int* flag);            // returns once *flag != 0
