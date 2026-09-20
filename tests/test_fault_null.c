// USE: fault
// A store through a null pointer lands on the reset vector, which is protected: it is a MemoryFault,
// so the mistake shows up at the store instead of as a program that merely misbehaves later.
#include "stdio.h"
#include "ceres/sys.h"

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    putstr("hook: interrupt ");
    putint(irq);
    putstr("\n");
}

static int* find(int wanted)
{
    return wanted ? (int*)0 : (int*)0;              // stands in for a lookup that found nothing
}

int main(void)
{
    putstr("installing\n");
    sys_install_fault_handlers(hook);
    putstr("storing through null\n");
    int* p = find(1);
    *p = 42;                                        // MemoryFault: never completes
    putstr("NOT REACHED\n");
    return 0;
}
