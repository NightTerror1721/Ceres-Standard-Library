#include "ceres/irq.h"
#include "ceres.h"

// A software vector table. See ceres/irq.h for why it exists and why linking it is optional.
//
// A handler has no parameters, so the vector alone cannot say which interrupt it answers: each
// device vector gets its own tiny __interrupt stub that calls the shared dispatcher with its number.

static irq_handler_t irq_table[IRQ_COUNT];

int irq_attach(int n, irq_handler_t handler)
{
    if (n < IRQ_USER_FIRST || n > IRQ_DEVICE_LAST)
        return -1;                 // no stub is bound to this number, so it would never be called
    irq_table[n] = handler;
    return 0;
}

void irq_detach(int n)
{
    if (n >= IRQ_USER_FIRST && n <= IRQ_DEVICE_LAST)
        irq_table[n] = 0;
}

irq_handler_t irq_handler(int n)
{
    if (n < 0 || n >= IRQ_COUNT)
        return 0;
    return irq_table[n];
}

static void irq_dispatch(int n)
{
    irq_handler_t handler = irq_table[n];
    if (handler)
        handler(n);
}

// One stub and one binding per device vector. The stub is `__interrupt`, so the compiler saves and
// restores every register around the call and ends with iret.
#define IRQ_STUB(N) \
    __interrupt void __irq_stub_##N(void) { irq_dispatch(N); } \
    __interrupt_vector(N, __irq_stub_##N);

IRQ_STUB(16)   // timer
IRQ_STUB(17)   // terminal input
IRQ_STUB(18)   // DMA transfer complete
IRQ_STUB(19)   // keyboard event
IRQ_STUB(20)   // mouse motion
IRQ_STUB(21)   // gamepad state change
IRQ_STUB(22)   // a tone has finished
IRQ_STUB(23)   // a medium was plugged in or pulled out
IRQ_STUB(24)   // the timer's alarm

const char* irq_name(int n)
{
    switch (n)
    {
        case 0:  return "Reset";
        case 1:  return "Trap";
        case 2:  return "IllegalInstruction";
        case 3:  return "MemoryFault";
        case 4:  return "DivisionByZero";
        case 5:  return "StackOverflow";
        case 6:  return "AlignmentFault";
        case 7:  return "PageFault";
        case 15: return "Syscall";
        case 16: return "Timer";
        case 17: return "Terminal";
        case 18: return "Dma";
        case 19: return "Keyboard";
        case 20: return "Mouse";
        case 21: return "Gamepad";
        case 22: return "Audio";
        case 23: return "Periph";
        case 24: return "Alarm";
        default: break;
    }
    if (n >= IRQ_USER_FIRST && n < IRQ_COUNT)
        return "UserInterrupt";
    return "Reserved";
}

void irq_enable_all(void)
{
    __builtin_sti();
}

// irq_wait is in asm/sys.casm: it has to be an `sti` and a `halt` with nothing between them.

void irq_wait_flag(volatile int* flag)
{
    // Interrupts stay masked while the flag is read, and only `sti; halt` opens them: an interrupt that
    // arrives after the check is held until the halt has run, and wakes it. Nothing can be lost between
    // looking and sleeping, so no guard timer is needed.
    unsigned int was = irq_save();
    while (*flag == 0)
    {
        irq_wait();
        __builtin_cli();
    }
    irq_restore(was);
}
