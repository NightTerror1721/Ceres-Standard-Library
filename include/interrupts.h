#pragma once

// Interrupt number space. 0-15 are reserved and always deliverable; 16-63 are
// user interrupts, delivered only while unmasked (__builtin_sti()).
// See Ceres-C docs/10-Interrupts.md and CeresASM docs/08-Interrupts-and-Exceptions.md.

#define IRQ_COUNT        64     // the size of the vector table
#define IRQ_USER_FIRST   16     // 0-15 are exceptions, always deliverable; 16-63 are masked unless sti
#define IRQ_USER_LAST    63
#define IRQ_DEVICE_LAST  24     // the last number a device raises today (the timer's alarm)

enum IRQ
{
    IRQ_TRAP        = 1,
    IRQ_ILLEGAL     = 2,
    IRQ_MEMFAULT    = 3,
    IRQ_DIV0        = 4,
    IRQ_STACK_OVF   = 5,
    IRQ_ALIGN       = 6,
    IRQ_PAGEFAULT   = 7,
    IRQ_SYSCALL     = 15,
    IRQ_TIMER       = 16,   // UserInterrupt0
    IRQ_TERMINAL    = 17,   // UserInterrupt1
    IRQ_DMA         = 18,   // UserInterrupt2
    IRQ_KEYBOARD    = 19,   // UserInterrupt3
    IRQ_MOUSE       = 20,   // UserInterrupt4
    IRQ_GAMEPAD     = 21,   // UserInterrupt5
    IRQ_AUDIO       = 22,   // UserInterrupt6: a tone has finished
    IRQ_PERIPH      = 23,   // UserInterrupt7: a medium was plugged in or pulled out
    IRQ_ALARM       = 24    // UserInterrupt8: the timer's alarm instant has come (ceres/timer.h)
};

// A handler is declared with `__interrupt` and entered only through the vector
// table - never called from C - so it has no callable function-pointer type.
// Declare it by hand (or via decl_interrupt_handler) and bind it in exactly one
// translation unit with __interrupt_vector(...).
#define decl_interrupt_handler(_Name) __interrupt void _Name(void)
#define decl_interrupt_vector(_Interrupt_id, _Handler) __interrupt_vector(_Interrupt_id, _Handler)
