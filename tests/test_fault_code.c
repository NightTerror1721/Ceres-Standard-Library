// USE: fault
// The program's own code is read-only: storing into it is a MemoryFault. (test_fault_vector and
// test_fault_null cover the vector table and the BIOS below the image.)
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
    putstr("storing into main\n");
    volatile unsigned int* code = (volatile unsigned int*)main;
    *code = 0;                                      // MemoryFault: never completes
    putstr("NOT REACHED\n");
    return 0;
}
