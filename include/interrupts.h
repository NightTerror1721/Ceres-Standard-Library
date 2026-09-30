#pragma once

// Interrupt number space. 0-15 are reserved and always deliverable; 16-63 are
// user interrupts, delivered only while unmasked (__builtin_sti()).
// See Ceres-C docs/10-Interrupts.md and CeresASM docs/08-Interrupts-and-Exceptions.md.

#define IRQ_COUNT        64     // the size of the vector table
#define IRQ_USER_FIRST   16     // 0-15 are exceptions, always deliverable; 16-63 are masked unless sti
#define IRQ_USER_LAST    63
#define IRQ_DEVICE_LAST  35     // the last number a device raises today (the GPU's fault)

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
    // The devices', by group (CeresASM docs/08-Interrupts-and-Exceptions.md). The numbers between are
    // reserved: 23, 24 and 25 for the disk and the host files, which raise nothing yet, 27, 29-31, 36-63.
    IRQ_TIMER       = 16,   // UserInterrupt0: the timer's countdown has run out
    IRQ_ALARM       = 17,   // UserInterrupt1: the timer's alarm instant has come (ceres/timer.h)
    IRQ_DMA         = 18,   // UserInterrupt2: a transfer has landed
    IRQ_TERMINAL    = 19,   // UserInterrupt3: input has arrived
    IRQ_KEYBOARD    = 20,   // UserInterrupt4
    IRQ_MOUSE       = 21,   // UserInterrupt5
    IRQ_GAMEPAD     = 22,   // UserInterrupt6
    IRQ_PERIPH      = 26,   // UserInterrupt10: a medium was plugged in or pulled out
    IRQ_AUDIO       = 28,   // UserInterrupt12: a tone has finished
    IRQ_VBLANK      = 32,   // UserInterrupt16: the GPU's vertical blank (ceres/video.h)
    IRQ_LINE        = 33,   // UserInterrupt17: the GPU's scan has reached its LineCompare line
    IRQ_BLITTER     = 34,   // UserInterrupt18: a blitter operation, or the GPU's copy engine, is done (ceres/blitter.h)
    IRQ_GPU_FAULT   = 35    // UserInterrupt19: the GPU was given an address outside the RAM and the VRAM
};

// A handler is declared with `__interrupt` and entered only through the vector
// table - never called from C - so it has no callable function-pointer type.
// Declare it by hand (or via decl_interrupt_handler) and bind it in exactly one
// translation unit with __interrupt_vector(...).
#define decl_interrupt_handler(_Name) __interrupt void _Name(void)
#define decl_interrupt_vector(_Interrupt_id, _Handler) __interrupt_vector(_Interrupt_id, _Handler)
