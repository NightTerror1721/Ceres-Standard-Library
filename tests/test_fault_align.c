// USE: fault
// A misaligned word load is an AlignmentFault. With the fault module linked it reaches our hook, which reports
// through putstr only (the handler runs on the 1 KiB system stack) and lets the machine stop.
#include "stdio.h"
#include "ceres/sys.h"

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    struct sys_layout l;
    sys_get_layout(&l);
    putstr("hook: interrupt ");
    putint(irq);
    putstr(pc >= l.text_start && pc < l.text_end ? ", pc inside the text section\n" : ", pc OUTSIDE the text section\n");
}

static char buffer[16];

int main(void)
{
    putstr("installing\n");
    sys_install_fault_handlers(hook);
    putstr("loading a word from an odd address\n");
    volatile unsigned int* odd = (volatile unsigned int*)(buffer + 1);
    unsigned int v = *odd;                          // AlignmentFault: never completes
    putstr("NOT REACHED\n");
    return (int)v;
}
