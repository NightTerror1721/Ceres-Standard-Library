// The C half of the fault reporter (see asm/optional/fault.casm, which binds the vectors).
#include "ceres/sys.h"
#include "ceres/backtrace.h"
#include "stdio.h"

extern void __fault_present(void);         // in fault.casm: referencing it links the vector bindings
extern unsigned int __fault_target;         // in fault.casm: where the handlers jump to

extern int __signal_hw_ready;               // src/signal.c: the fault vectors are bound, so signal() may take handlers for them
int __signal_of_fault(int irq);             // ... which signal a fault is
int __signal_deliver(int sig);              // ... runs its handler; 0 when there is none

static fault_hook_t user_hook;
static unsigned int fault_frame;
static void fault_report(int irq, unsigned int pc, unsigned int flags, unsigned int fp);

unsigned int sys_fault_frame(void)
{
    return fault_frame;
}

void sys_install_fault_handlers(fault_hook_t hook)
{
    user_hook = hook;
    __fault_target = (unsigned int)fault_report;
    __fault_present();
    __signal_hw_ready = 1;
}

static const char* fault_name(int irq)
{
    switch (irq)
    {
    case 1: return "Trap";
    case 2: return "IllegalInstruction";
    case 3: return "MemoryFault";
    case 4: return "DivisionByZero";
    case 5: return "StackOverflow";
    case 6: return "AlignmentFault";
    case 7: return "PageFault";
    }
    return "Interrupt";
}

// Called from the handlers, on the system stack, with interrupts masked. It reports through putstr and
// puthex (no printf: the format engine needs stack, and a stack overflow is one of the faults reported).
//
// A program with a signal handler for the fault (signal.h) gets it first. Returning from this function goes
// back to the program, and only the division by zero stub is written to do that (fault.casm): for it a handler
// that returns means "carry on after the division". Every other stub never returns, so the fault is reported
// and the program stops whatever the handler did.
// "store 2 bytes to 0x10001", from the machine's fault registers: only a memory fault sets them.
static void report_access(void)
{
    unsigned int access = sys_fault_access();
    unsigned int kind = access & 0xFFu;
    unsigned int size = access >> 8;                 // bits 8-31: a block instruction's chunk runs to a page
    if (kind == 0u || access == 0xFFFFFFFFu)
        return;                                          // nothing recorded, or no such register
    putstr(kind == FAULT_WRITE ? "  store" : kind == FAULT_FETCH ? "  fetch" : "  load");
    if (size != 0u)
    {
        putstr(" ");
        putint((int)size);
        putstr(size == 1u ? " byte" : " bytes");
    }
    putstr(kind == FAULT_WRITE ? " to " : " from ");
    puthex(sys_fault_address());
    putstr("\n");
}

static void fault_report(int irq, unsigned int pc, unsigned int flags, unsigned int fp)
{
    fault_frame = fp;
    int sig = __signal_of_fault(irq);
    if (sig != 0 && __signal_deliver(sig) && irq == 4)
    {
        fault_frame = 0;                                 // the program carries on: no fault in progress
        return;
    }
    if (user_hook != 0)
    {
        user_hook(irq, pc, flags);
    }
    else
    {
        putstr("\nfault: ");
        putstr(fault_name(irq));
        putstr(" (interrupt ");
        putint(irq);
        putstr(") at ");
        puthex(pc);
        const char* name = backtrace_symbol(pc, 0);
        if (name != 0)
        {
            putstr(" (");
            backtrace_print_address(pc);
            putstr(")");
        }
        putstr(", flags ");
        puthex(flags);
        putstr("\n");
        if (irq == 3 || irq == 6 || irq == 7)
            report_access();
        if (backtrace_has_symbols())                     // the callers, whether or not the pc itself has a name
        {
            putstr("called from:\n");
            backtrace_print_from(fp);
        }
    }
    sys_exit_status(139);                                // a fault ends the program the way a segmentation fault does
}
