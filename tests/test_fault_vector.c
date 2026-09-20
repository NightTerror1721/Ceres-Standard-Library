// USE: fault
// The interrupt vector table (0x00-0xFF) and the BIOS (0x100-0x3FF) are read-only: a store into either
// is a MemoryFault, not a store that quietly does nothing.
#include "stdio.h"
#include "ceres/sys.h"

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    putstr("hook: interrupt ");
    putint(irq);
    putstr("\n");
}

int main(void)
{
    putstr("installing\n");
    sys_install_fault_handlers(hook);
    putstr("storing into the BIOS\n");
    volatile unsigned int* bios = (volatile unsigned int*)0x180;
    *bios = 0;                                      // MemoryFault: never completes
    putstr("NOT REACHED\n");
    return 0;
}
