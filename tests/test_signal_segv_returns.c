// USE: fault
// A handler for SIGSEGV that returns does not get the program back: repeating the store would fault for ever.
// The fault is reported (here through a hook, so no address appears) and the program stops with 139
// (tests/expected/test_signal_segv_returns.status).
#include "stdio.h"
#include "ceres/sys.h"
#include "signal.h"

static void on_segv(int sig)
{
    putstr("handler returns: signal ");                  // no printf: this runs on the 4 KiB system stack
    putint(sig);
    putstr("\n");
}

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    putstr("reported: interrupt ");
    putint(irq);
    putstr("\n");
}

int main(void)
{
    sys_install_fault_handlers(hook);
    signal(SIGSEGV, on_segv);
    *(volatile unsigned int*)0x10 = 1;
    puts("NOT REACHED");
    return 0;
}
