// The C half of the fault reporter (see asm/optional/fault.casm, which binds the vectors).
#include "ceres/sys.h"
#include "stdio.h"

extern void __fault_present(void);         // in fault.casm: referencing it links the vector bindings
extern unsigned int __fault_target;         // in fault.casm: where the handlers jump to

static fault_hook_t user_hook;
static void fault_report(int irq, unsigned int pc, unsigned int flags);

void sys_install_fault_handlers(fault_hook_t hook)
{
    user_hook = hook;
    __fault_target = (unsigned int)fault_report;
    __fault_present();
}

static const char* fault_name(int irq)
{
    switch (irq)
    {
    case 1: return "Trap";
    case 2: return "IllegalInstruction";
    case 3: return "MemoryFault";
    case 5: return "StackOverflow";
    case 6: return "AlignmentFault";
    case 7: return "PageFault";
    }
    return "Interrupt";
}

// Called from the handlers, on the system stack, with interrupts masked. It reports through putstr and
// puthex (no printf: the format engine needs stack, and a stack overflow is one of the faults reported).
static void fault_report(int irq, unsigned int pc, unsigned int flags)
{
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
        putstr(", flags ");
        puthex(flags);
        putstr("\n");
    }
    sys_exit_status(139);                                // a fault ends the program the way a segmentation fault does
}
